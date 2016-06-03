/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This file is part of REDHAWK Basic Components psd.
 *
 * REDHAWK Basic Components psd is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * REDHAWK Basic Components psd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this
 * program.  If not, see http://www.gnu.org/licenses/.
 */

/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "psd.h"

PREPARE_LOGGING(PsdProcessor)
PREPARE_LOGGING(psd_i)

/****************************************************************
 ****************************************************************
 **                                                            **
 **                     Helper Methods                         **
 **                                                            **
 ****************************************************************
 ****************************************************************/

void copyVec(const bulkio::FloatDataBlock &block, RealFFTWVector &out){
    out.resize(block.size());
    memcpy(&out[0], block.data(), out.size()*sizeof(float));
}

void copyVec(const bulkio::FloatDataBlock &block, ComplexFFTWVector &out){
    out.resize(block.cxsize());
    memcpy(&out[0], block.data(), out.size()*sizeof(std::complex<float>));
}

/****************************************************************
 ****************************************************************
 **                                                            **
 **                   PsdProcessor class                       **
 **                                                            **
 ****************************************************************
 ****************************************************************/
PsdProcessor::PsdProcessor(bulkio::InFloatStream inStream,
                    bulkio::OutFloatStream fftStream,
                    bulkio::OutFloatStream psdStream,
                    size_t fftSize,
                    int overlap,
                    size_t numAvg,
                    float logCoeff,
                    bool doFFT,
                    bool doPSD,
                    bool rfFreqUnits,
                    float delay) :
        ThreadedComponent(),
        in(inStream),
        outFFT(fftStream),
        outPSD(psdStream),
        realPsd_(NULL),
        complexPsd_(NULL),
        vecMean_(numAvg, psdOut_, psdAverage_),
        eos(false),
        paramLock(new boost::mutex()){
    LOG_DEBUG(PsdProcessor,__PRETTY_FUNCTION__<<" streamID="<<in.streamID());
    params.fftSz = fftSize;
    params.fftSzChanged = true;
    params.strideSize=fftSize-overlap;
    params.numAverage = numAvg;
    params.numAverageChanged = true;
    params.overlap = overlap;
    params.doFFT = doFFT;
    params.doPSD = doPSD;
    params.rfFreqUnits = rfFreqUnits;
    params.logCoeff = logCoeff;
    params.updateSRI = true; // force initial SRI push
    setThreadDelay(delay);
    ThreadedComponent::startThread();
}
PsdProcessor::~PsdProcessor(){
    LOG_DEBUG(PsdProcessor,__PRETTY_FUNCTION__<<" streamID="<<in.streamID());
    if(!!outFFT){
        outFFT.close();
    }
    if(!!outPSD){
        outPSD.close();
    }
    flush();
}

void PsdProcessor::updateFftSize(size_t fftSize){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" streamID="<<in.streamID());
    boost::mutex::scoped_lock lock(*paramLock);
    params.fftSz=fftSize;
    params.strideSize=fftSize-params.overlap;
    params.fftSzChanged = true;
    params.updateSRI=true;
}
void PsdProcessor::updateOverlap(int overlap){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" streamID="<<in.streamID());
    boost::mutex::scoped_lock lock(*paramLock);
    params.overlap = overlap;
    params.strideSize=params.fftSz-overlap;
    params.updateSRI=true;

}
void PsdProcessor::updateNumAvg(size_t avg){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" streamID="<<in.streamID());
    boost::mutex::scoped_lock lock(*paramLock);
    params.numAverage = avg;
    params.numAverageChanged = true;
    params.updateSRI=true;
}

void PsdProcessor::forceSRIUpdate(){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" streamID="<<in.streamID());
    boost::mutex::scoped_lock lock(*paramLock);
    params.updateSRI=true;
}

void PsdProcessor::updateActions(bool psd, bool fft){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" psd:"<<psd<<" fft:"<<fft);
    boost::mutex::scoped_lock lock(*paramLock);
    params.doPSD = psd;
    params.doFFT = fft;
}

void PsdProcessor::updateRfFreqUnits(bool enable){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" new value is "<<enable);
    boost::mutex::scoped_lock lock(*paramLock);
    params.rfFreqUnits = enable;
    params.updateSRI=true;
}

void PsdProcessor::updateLogCoefficient(float logCoeff){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__<<" new value is "<<logCoeff);
    boost::mutex::scoped_lock lock(*paramLock);
    params.logCoeff = logCoeff;
}

bool PsdProcessor::finished(){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__);
    return eos;
}

void PsdProcessor::stop() throw (CORBA::SystemException, CF::Resource::StopError){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__);
    if (!ThreadedComponent::stopThread()) {
        throw CF::Resource::StopError(CF::CF_NOTSET, "PsdProcessor thread did not die");
    }
}

void PsdProcessor::flush(){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__);
    boost::mutex::scoped_lock lock(*paramLock);
    //delete the pointers - then on next data call when we start processing again
    //the rest of the processing state is flushed
    if (realPsd_!=NULL)
        delete realPsd_;
    if (complexPsd_!=NULL)
        delete complexPsd_;

    realPsd_ = NULL;
    complexPsd_ = NULL;
}

int PsdProcessor::serviceFunction(){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__);

    // update cached copy of params
    {
        boost::mutex::scoped_lock lock(*paramLock);

        // update cache
        bool tmp = params_cache.updateSRI;
        params_cache = params;
        params_cache.updateSRI = params.updateSRI || tmp; // preserve flag if not addressed yet
        // Note: we do not need to preserve the other change flags b/c they are always addressed

        // reset global
        params.fftSzChanged = false;
        params.numAverageChanged = false;
        params.updateSRI = false; // always reset to false once addressed
    }

    // update all data structures before processing, if needed
    if(params_cache.fftSzChanged){
        LOG_TRACE(PsdProcessor,"serviceFunction - updating data structures due to new fft size");
        params_cache.fftSzChanged = false;
        if (realPsd_){
            realIn_.resize(params_cache.fftSz);
            psdOut_.resize(params_cache.fftSz/2+1);
            fftOut_.resize(params_cache.fftSz/2+1);
            realPsd_->setLength(params_cache.fftSz);
        } else if (complexPsd_) {
            complexIn_.resize(params_cache.fftSz);
            psdOut_.resize(params_cache.fftSz);
            fftOut_.resize(params_cache.fftSz);
            complexPsd_->setLength(params_cache.fftSz);
        }
    }

    if(params_cache.numAverageChanged){
        LOG_TRACE(PsdProcessor,"serviceFunction - updating data structures due to new num average");
        params_cache.numAverageChanged = false;
        vecMean_.setAvgNum(params_cache.numAverage);
    }

    bulkio::FloatDataBlock block = in.tryread(params_cache.fftSz,params_cache.strideSize);

    if (!block) {
        if( in.eos()){
            LOG_DEBUG(PsdProcessor,"serviceFunction - got null block with EOS");
            eos=true;
            return FINISH;
        } else {
            LOG_DEBUG(PsdProcessor,"serviceFunction - got null block without EOS");
            return NOOP;
        }
    }
    LOG_DEBUG(PsdProcessor,"serviceFunction - got block of size "<<block.size());

    if (block.inputQueueFlushed()) {
        LOG_WARN(PsdProcessor, "Input queue flushed.  Flushing internal buffers.");
        //flush all our processor states if the queue flushed
        flush();
    }

    // TODO - what if we got a partial block w/o fftSz worth of data?

    // do work and push out data
    if (block.complex()) {

        // setup Complex
        if (realPsd_){
            delete realPsd_;
            realPsd_=NULL;
        }
        if (complexPsd_==NULL){
            complexPsd_ = new ComplexPsd(complexIn_, psdOut_, fftOut_, params_cache.fftSz,true);
            vecMean_.clear();
        }

        copyVec(block,complexIn_);
        complexPsd_->run();

    } else {

        // setup Scalar
        if (complexPsd_){
            delete complexPsd_;
            complexPsd_=NULL;
        }
        if (realPsd_==NULL){
            realPsd_ = new RealPsd(realIn_, psdOut_, fftOut_, params_cache.fftSz,true);
            vecMean_.clear();
        }

        copyVec(block,realIn_);
        realPsd_->run();
    }

    float* psdOutPtr = NULL;
    size_t psdOutLen = 0;
    if (params_cache.doPSD){
        if (params_cache.numAverage > 1){
            if (vecMean_.run()){
                psdOutPtr = &psdAverage_[0];
                psdOutLen = psdAverage_.size();
            }
        } else {
            psdOutPtr = &psdOut_[0];
            psdOutLen = psdOut_.size();
        }
        //take the log of the output if necessary
        if (params_cache.logCoeff > 0){
            for (size_t i=0;i<psdOutLen;i++){
                psdOutPtr[i]=params_cache.logCoeff*log10(psdOutPtr[i]);
            }
        }
    }

    std::complex<float>* fftOutPtr = NULL;
    size_t fftOutLen = 0;
    if (params_cache.doFFT){
        fftOutPtr = &fftOut_[0];
        fftOutLen = fftOut_.size();
    }

    // Update SRI
    if (params_cache.updateSRI || block.sriChanged()) {
        params_cache.updateSRI = false; // always reset to false once addressed
        updateSRI(block);
    }

    //output data
    // NOTE - getTimeStamps() returns sorted list.
    //        First is guaranteed to be offset 0, and may or may not be synthetic.
    //        If any others, they will be non-synthetic.
    // TODO - should adjust Timestamp for extra sample delay from elements in last loop
    if (psdOutLen>0){
        // we can assume params_cache.doPSD=true and psdOutPtr!=NULL if psdOutLen>0
        outPSD.write(psdOutPtr, psdOutLen, block.getTimestamps().front().time);
    }
    if (fftOutLen>0){
        // we can assume params_cache.doFFT=true and fftOutPtr!=NULL if fftOutLen>0
        outFFT.write(fftOutPtr, fftOutLen, block.getTimestamps().front().time);
    }

    if (in.eos()){
        LOG_TRACE(PsdProcessor,"serviceFunction - got EOS");
        eos=true;
        return FINISH;
    }

    return NORMAL;
}

void PsdProcessor::updateSRI(const bulkio::FloatDataBlock &block){
    LOG_TRACE(PsdProcessor,__PRETTY_FUNCTION__);

    // example of how to use sriChangeFlags
    if (block.sriChangeFlags() & bulkio::sri::XDELTA) {
        LOG_DEBUG(PsdProcessor,"SRI.xdelta changed");
    } else if (block.sriChangeFlags() & bulkio::sri::MODE) {
        LOG_DEBUG(PsdProcessor,"SRI.mode changed");
    }

    BULKIO::StreamSRI outputSRI;

    // Pass along any keywords that were in the source
    outputSRI.keywords.length(block.sri().keywords.length());

    for (size_t i = 0; i < block.sri().keywords.length(); ++i) {
        outputSRI.keywords[i] = block.sri().keywords[i];
    }

    double xdelta_in = block.xdelta();
    outputSRI.xdelta = 1.0/(xdelta_in*params_cache.fftSz);

    double ifStart = 0;
    if (block.complex()) //complex Data
        ifStart = -((params_cache.fftSz/2-1)*outputSRI.xdelta);

    //adjust the xstart for RF units if required
    if (params_cache.rfFreqUnits){
        const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(block.sri().keywords);
        long rfCenter;
        bool validRF = false;
        if(props.find("CHAN_RF")!=props.end()){
            rfCenter = props["CHAN_RF"].toLong();
            validRF = true;
        } else if(props.find("COL_RF")!=props.end()){
            rfCenter = props["COL_RF"].toLong();
            validRF = true;
        }
        if (validRF){
            double ifCentre=0;
            if (!block.complex()) //real data is at fs/4.0
                ifCentre = 1.0/xdelta_in/4.0;
            double deltaF = rfCenter-ifCentre; //Translation between rf & if
            outputSRI.xstart = ifStart+deltaF;  //This the the start bin at RF
        } else {
            LOG_WARN(PsdProcessor, "rf Frequency units requested but no rf unit keyword present");
            outputSRI.xstart = ifStart;
        }
    } else {
        outputSRI.xstart = ifStart;
    }

    if (!block.complex())
        outputSRI.subsize = params_cache.fftSz/2+1;
    else
        outputSRI.subsize =params_cache.fftSz;
    outputSRI.ydelta = xdelta_in*params_cache.strideSize;
    outputSRI.yunits = BULKIO::UNITS_TIME;
    outputSRI.xunits = BULKIO::UNITS_FREQUENCY;
    outputSRI.mode = 1; //data is always complex out of the fft

    // set/update the sri for the output FFT stream
    outFFT.sri(outputSRI);

    if (params_cache.numAverage > 2)
        outputSRI.ydelta*=params_cache.numAverage;

    // set/update the sri for the output PSD stream
    outputSRI.mode = 0; //data is always real out of the psd
    outPSD.sri(outputSRI);

}

/****************************************************************
 ****************************************************************
 **                                                            **
 **                       psd_i class                          **
 **                                                            **
 ****************************************************************
 ****************************************************************/
psd_i::psd_i(const char *uuid, const char *label) :
   psd_base(uuid, label),
   doPSD(false),
   doFFT(false),
   listener(*this, &psd_i::callBackFunc)
{
    psd_dataFloat_out->setNewConnectListener(&listener);
    fft_dataFloat_out->setNewConnectListener(&listener);
}

psd_i::~psd_i()
{
    clearThreads();
}

void psd_i::constructor()
{
    /***********************************************************************************
     This is the RH constructor. All properties are properly initialized before this function is called
    ***********************************************************************************/
    addPropertyListener(fftSize, this, &psd_i::fftSizeChanged);
    addPropertyListener(overlap, this, &psd_i::overlapChanged);
    addPropertyListener(numAvg, this, &psd_i::numAvgChanged);
    addPropertyListener(rfFreqUnits, this, &psd_i::rfFreqUnitsChanged);
    addPropertyListener(logCoefficient, this, &psd_i::logCoeffChanged);

    dataFloat_in->addStreamListener(this, &psd_i::streamAdded);
}
/***********************************************************************************************

    Basic functionality:

        The service function is called by the serviceThread object (of type ProcessThread).
        This call happens immediately after the previous call if the return value for
        the previous call was NORMAL.
        If the return value for the previous call was NOOP, then the serviceThread waits
        an amount of time defined in the serviceThread's constructor.

    SRI:
        To create a StreamSRI object, use the following code:
                std::string stream_id = "testStream";
                BULKIO::StreamSRI sri = bulkio::sri::create(stream_id);

    Time:
        To create a PrecisionUTCTime object, use the following code:
                BULKIO::PrecisionUTCTime tstamp = bulkio::time::utils::now();


    Ports:

        Data is passed to the serviceFunction through the getPacket call (BULKIO only).
        The dataTransfer class is a port-specific class, so each port implementing the
        BULKIO interface will have its own type-specific dataTransfer.

        The argument to the getPacket function is a floating point number that specifies
        the time to wait in seconds. A zero value is non-blocking. A negative value
        is blocking.  Constants have been defined for these values, bulkio::Const::BLOCKING and
        bulkio::Const::NON_BLOCKING.

        Each received dataTransfer is owned by serviceFunction and *MUST* be
        explicitly deallocated.

        To send data using a BULKIO interface, a convenience interface has been added
        that takes a std::vector as the data input

        NOTE: If you have a BULKIO dataSDDS port, you must manually call
              "port->updateStats()" to update the port statistics when appropriate.

        Example:
            // this example assumes that the component has two ports:
            //  A provides (input) port of type bulkio::InShortPort called short_in
            //  A uses (output) port of type bulkio::OutFloatPort called float_out
            // The mapping between the port and the class is found
            // in the component base class header file

            bulkio::InShortPort::dataTransfer *tmp = short_in->getPacket(bulkio::Const::BLOCKING);
            if (not tmp) { // No data is available
                return NOOP;
            }

            std::vector<float> outputData;
            outputData.resize(tmp->dataBuffer.size());
            for (unsigned int i=0; i<tmp->dataBuffer.size(); i++) {
                outputData[i] = (float)tmp->dataBuffer[i];
            }

            // NOTE: You must make at least one valid pushSRI call
            if (tmp->sriChanged) {
                float_out->pushSRI(tmp->SRI);
            }
            float_out->pushPacket(outputData, tmp->T, tmp->EOS, tmp->streamID);

            delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
            return NORMAL;

        If working with complex data (i.e., the "mode" on the SRI is set to
        true), the std::vector passed from/to BulkIO can be typecast to/from
        std::vector< std::complex<dataType> >.  For example, for short data:

            bulkio::InShortPort::dataTransfer *tmp = myInput->getPacket(bulkio::Const::BLOCKING);
            std::vector<std::complex<short> >* intermediate = (std::vector<std::complex<short> >*) &(tmp->dataBuffer);
            // do work here
            std::vector<short>* output = (std::vector<short>*) intermediate;
            myOutput->pushPacket(*output, tmp->T, tmp->EOS, tmp->streamID);

        Interactions with non-BULKIO ports are left up to the component developer's discretion

    Properties:

        Properties are accessed directly as member variables. For example, if the
        property name is "baudRate", it may be accessed within member functions as
        "baudRate". Unnamed properties are given a generated name of the form
        "prop_n", where "n" is the ordinal number of the property in the PRF file.
        Property types are mapped to the nearest C++ type, (e.g. "string" becomes
        "std::string"). All generated properties are declared in the base class
        (psd_base).

        Simple sequence properties are mapped to "std::vector" of the simple type.
        Struct properties, if used, are mapped to C++ structs defined in the
        generated file "struct_props.h". Field names are taken from the name in
        the properties file; if no name is given, a generated name of the form
        "field_n" is used, where "n" is the ordinal number of the field.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue
            //  - A boolean called scaleInput

            if (scaleInput) {
                dataOut[i] = dataIn[i] * scaleValue;
            } else {
                dataOut[i] = dataIn[i];
            }

        A callback method can be associated with a property so that the method is
        called each time the property value changes.  This is done by calling
        setPropertyChangeListener(<property name>, this, &psd_i::<callback method>)
        in the constructor.

        Example:
            // This example makes use of the following Properties:
            //  - A float value called scaleValue

        //Add to psd.cpp
        psd_i::psd_i(const char *uuid, const char *label) :
            psd_base(uuid, label)
        {
            setPropertyChangeListener("scaleValue", this, &psd_i::scaleChanged);
        }

        void psd_i::scaleChanged(const std::string& id){
            std::cout << "scaleChanged scaleValue " << scaleValue << std::endl;
        }

        //Add to psd.h
        void scaleChanged(const std::string&);


************************************************************************************************/
int psd_i::serviceFunction()
{
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);

    // clean up finished threads
    int retval = NOOP;
    {
        boost::mutex::scoped_lock lock(stateMapLock);
        for(map_type::iterator i = stateMap.begin();i!=stateMap.end();){
            if( i->second->finished() ){
                LOG_DEBUG(psd_i,"Removing thread processor (eos): "<<i->first);
                stateMap.erase(i++);
                retval = NORMAL;
            } else {
                ++i;
            }
        }
    }

    return retval;
}

void psd_i::streamAdded(bulkio::InFloatStream stream){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    boost::mutex::scoped_lock lock(stateMapLock);
    if (stateMap.find(stream.streamID())==stateMap.end()){
        LOG_DEBUG(psd_i,"Adding new thread processor: "<<stream.streamID());
        bulkio::OutFloatStream outputFFT = fft_dataFloat_out->createStream(stream.streamID());
        bulkio::OutFloatStream outputPSD = psd_dataFloat_out->createStream(stream.streamID());
        boost::shared_ptr<PsdProcessor> newThread(
                new PsdProcessor(stream, outputFFT, outputPSD, fftSize, overlap, numAvg,
                        logCoefficient, doFFT, doPSD, rfFreqUnits));
        map_type::value_type newEntry(stream.streamID(),newThread);
        stateMap.insert(stateMap.end(),newEntry);
    } else {
        LOG_WARN(psd_i,"New stream with stream ID "<<stream.streamID()<<", but already have entry for that stream ID");
    }
}

void psd_i::stop() throw (CORBA::SystemException, CF::Resource::StopError){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    clearThreads();
    psd_base::stop();
}

void psd_i::clearThreads(){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    {
        boost::mutex::scoped_lock lock(stateMapLock);
        for(map_type::iterator i = stateMap.begin();i!=stateMap.end();i++){
            i->second->stop();
        }
        stateMap.clear();
    }
}

void psd_i::fftSizeChanged(unsigned int oldValue, unsigned int newValue){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    if (oldValue != newValue) {
        boost::mutex::scoped_lock lock(stateMapLock);
        for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++) {
            i->second->updateFftSize(fftSize);
        }
    }
}

void psd_i::numAvgChanged(unsigned int oldValue, unsigned int newValue){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    if (oldValue != newValue) {
        boost::mutex::scoped_lock lock(stateMapLock);
        for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
            i->second->updateNumAvg(numAvg);
    }
}

void psd_i::overlapChanged(int oldValue, int newValue){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    if (oldValue != newValue) {
        boost::mutex::scoped_lock lock(stateMapLock);
        for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
            i->second->updateOverlap(overlap);
    }
}

void psd_i::rfFreqUnitsChanged(bool oldValue, bool newValue){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    if (oldValue != newValue) {
        boost::mutex::scoped_lock lock(stateMapLock);
        for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
            i->second->updateRfFreqUnits(rfFreqUnits);
    }
}

void psd_i::logCoeffChanged(float oldValue, float newValue){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    if (oldValue != newValue) {
        boost::mutex::scoped_lock lock(stateMapLock);
        for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
            i->second->updateLogCoefficient(logCoefficient);
    }
}

void psd_i::callBackFunc( const char* connectionId){
    LOG_TRACE(psd_i,__PRETTY_FUNCTION__);
    bool doUpdate = false;
    if(doPSD != (psd_dataFloat_out->state()!=BULKIO::IDLE)){
        doPSD = !doPSD;
        doUpdate = true;
    }
    if(doFFT != (fft_dataFloat_out->state()!=BULKIO::IDLE)){
        doFFT = !doFFT;
        doUpdate = true;
    }
    if(doUpdate){
        boost::mutex::scoped_lock lock(stateMapLock);
        for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
            i->second->updateActions(doPSD, doFFT);
    }
}
