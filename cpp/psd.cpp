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

PREPARE_LOGGING(psd_i)

//a few quick little helper methods here
void copyVec(std::vector<float>::iterator start, std::vector<float>::iterator stop, RealFFTWVector &out)
{
	out.assign(start,stop);
}

void copyVec(std::vector<float>::iterator start, std::vector<float>::iterator stop, ComplexFFTWVector &out)
{
	size_t newOutSize = (stop-start)/2;
	out.resize(newOutSize);
	memcpy(&out[0],&(*start), out.size()*sizeof(std::complex<float>));
}

template<typename T, typename U>
void appendVec(const std::vector<float, T>&in, std::vector<float, U> &out)
{
	copy(in.begin(),in.end(), back_inserter(out));
}

template<typename T, typename U>
void appendVec(const std::vector<std::complex<float> , T>&in, std::vector<std::complex<float>, U> &out)
{
	copy(in.begin(),in.end(), back_inserter(out));
}


template<typename T, typename U>
void appendVec(const std::vector<std::complex<float>, T>&in, std::vector<float, U> &out)
{
	size_t oldSize = out.size();
	size_t inSize = 2*in.size();
	out.resize(oldSize+inSize);
	memcpy(&out[oldSize],&in[0],inSize*sizeof(float));
}

PsdProcessor::PsdProcessor(std::vector<float>& psdOutVec,
					std::vector<float>& fftOutVec,
					size_t fftSize,
					int overlap,
					size_t numAvg) :
		psdOutVec_(psdOutVec),
		fftOutVec_(fftOutVec),
		frameBuffer_(fftSize,overlap),
		realPsd_(NULL),
		complexPsd_(NULL),
		vecMean_(numAvg, psdOut_, psdAverage_),
		overlap_(overlap)
{
	params_.fftSz = fftSize;
	params_.numAverage=false;
	params_.strideSize=fftSize-overlap;
	params_.numAverage = numAvg;
}
PsdProcessor::~PsdProcessor()
{
	flush();
}

void PsdProcessor::updateFftSize(size_t fftSize)
{
	if (realPsd_)
	{
		frameBuffer_.setFrameSize(fftSize);
		realIn_.resize(fftSize);
		psdOut_.resize(fftSize/2+1);
		fftOut_.resize(fftSize/2+1);
		realPsd_->setLength(fftSize);
	}
	else if (complexPsd_)
	{
		frameBuffer_.setFrameSize(2*fftSize);
		complexIn_.resize(fftSize);
		psdOut_.resize(fftSize);
		fftOut_.resize(fftSize);
		complexPsd_->setLength(fftSize);
	}
	params_.fftSz=fftSize;
	params_.strideSize=fftSize-overlap_;
	params_.updateSRI=true;
}
void PsdProcessor::updateOverlap(int overlap)
{
	long floatOverlap = overlap;
	if (complexPsd_)
	{
		floatOverlap=2*overlap;
	}
	frameBuffer_.setOverlap(floatOverlap);
	params_.updateSRI=true;
	params_.strideSize=params_.fftSz-overlap;
	overlap_ = overlap;

}
void PsdProcessor::updateNumAvg(size_t avg)
{
	params_.numAverage = avg;
	vecMean_.setAvgNum(avg);
	params_.updateSRI=true;
}

void PsdProcessor::forceSRIUpdate()
{
	params_.updateSRI=true;
}

void PsdProcessor::flush()
{
	//delete the pointers - then on next data call when we start processing again
	//the rest of the processing state is flushed
	if (realPsd_!=NULL)
		delete realPsd_;
	if (complexPsd_!=NULL)
		delete complexPsd_;

	realPsd_ = NULL;
	complexPsd_ = NULL;
}

PsdProcessor::ParamStruct PsdProcessor::process(std::vector<float>& input, bool cmplx, bool doPSD, bool doFFT, float logCoefficient)
{
	if (cmplx)
	{
		if (realPsd_)
		{
			delete realPsd_;
			realPsd_=NULL;
		}
		if (complexPsd_==NULL)
		{
			complexPsd_ = new ComplexPsd(complexIn_, psdOut_, fftOut_, params_.fftSz,true);
			frameBuffer_.flush();
			frameBuffer_.setFrameSize(params_.fftSz*2);
			frameBuffer_.setOverlap(2*overlap_);
			vecMean_.clear();
		}

	} else
	{
		if (complexPsd_)
		{
			delete complexPsd_;
			complexPsd_=NULL;
		}
		if (realPsd_==NULL)
		{
			realPsd_ = new RealPsd(realIn_, psdOut_, fftOut_, params_.fftSz,true);
			frameBuffer_.flush();
			frameBuffer_.setFrameSize(params_.fftSz);
			frameBuffer_.setOverlap(overlap_);
			vecMean_.clear();
		}
	}

	frameBuffer_.newData(input,framedData_);
	if (complexPsd_)
	{
		serviceLoop(complexPsd_, complexIn_,doPSD, doFFT, logCoefficient);
	}
	else if (realPsd_)
	{
		serviceLoop(realPsd_, realIn_,doPSD, doFFT, logCoefficient);
	}
	else
	{
		std::cerr<<"this shouldn't happen - no real or complex psd created"<<std::endl;
	}
	return params_;
}

template <typename TimeType>
void PsdProcessor::serviceLoop(Fft<TimeType>* psd, TimeType& psdInput, bool doPSD, bool doFFT, float logCoefficient)
{
	//make sure ther is input and we have an output hooked up
	if (!framedData_.empty())
	{
		psdOutVec_.clear();
		fftOutVec_.clear();

		for (unsigned int i=0; i!=framedData_.size(); i++)
		{
			copyVec(framedData_[i].begin,framedData_[i].end,psdInput);
			psd->run();
				if (params_.numAverage > 1)
				{
					if (vecMean_.run() && doPSD)
						appendVec(psdAverage_,psdOutVec_);
				}
				else if(doPSD)
					appendVec(psdOut_,psdOutVec_);
			if (doFFT)
				appendVec(fftOut_,fftOutVec_);
		}
		if (!psdOutVec_.empty() && logCoefficient >0)
		{
			//take the log of the output if necessary
			for (std::vector<float>::iterator i=psdOutVec_.begin(); i!=psdOutVec_.end(); i++)
				*i=logCoefficient*log10(*i);
		}
	}
}

psd_i::psd_i(const char *uuid, const char *label) :
   psd_base(uuid, label),
   doPSD(false),
   doFFT(false),
   listener(*this, &psd_i::callBackFunc)

{
	addPropertyChangeListener("fftSize", this, &psd_i::fftSizeChanged);
	addPropertyChangeListener("overlap", this, &psd_i::overlapChanged);
	addPropertyChangeListener("numAvg", this, &psd_i::numAvgChanged);
	addPropertyChangeListener("rfFreqUnits", this, &psd_i::rfFreqUnitsChanged);

	psd_dataFloat_out->setNewConnectListener(&listener);
	fft_dataFloat_out->setNewConnectListener(&listener);
}

psd_i::~psd_i()
{

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
    LOG_DEBUG(psd_i, "serviceFunction() example log message");

    bulkio::InFloatPort::dataTransfer *tmp = dataFloat_in->getPacket(-1);
	if (not tmp) { // No data is available
		return NOOP;
	}
	if (tmp->inputQueueFlushed)
	{
		LOG_WARN(psd_i, "Input queue flushed - data has been thrown on the floor.  Flushing internal buffers.");
		//flush all our processor states if the queue flushed
		boost::mutex::scoped_lock lock(psdLock_);
		for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
			i->second->flush();
	}
	bool updateSRI = false;
	PsdProcessor::ParamStruct params;
	{
		boost::mutex::scoped_lock lock(psdLock_);
		std::map<std::string, PsdProcessor*>::iterator i = stateMap.find(tmp->streamID);
		if (i==stateMap.end())
		{
			updateSRI = true;
			map_type::value_type processor(tmp->streamID, new PsdProcessor(psdOutVec_, fftOutVec_,fftSize, overlap, numAvg));
			i = stateMap.insert(stateMap.end(),processor);
		}

		//process with the PsdState right here
		params = i->second->process(tmp->dataBuffer, tmp->SRI.mode==1, doPSD, doFFT, logCoefficient);
		if (tmp->EOS)
		{
			delete i->second;
			stateMap.erase(i);
		}
	}
	if (params.updateSRI)
		updateSRI = true;

	// NOTE: You must make at least one valid pushSRI call
	if (tmp->sriChanged || updateSRI) {
		bool validRF = false;
		double xdelta = tmp->SRI.xdelta;
		tmp->SRI.xdelta = 1.0/(xdelta*params.fftSz);

		double ifStart = 0;
		if (tmp->SRI.mode==1) //complex Data
			ifStart = -((params.fftSz/2-1)*tmp->SRI.xdelta);

		//adjust the xstart for RF units if required
		if (rfFreqUnits)
		{
			long rfCentre = getKeywordByID<CORBA::Long>(tmp->SRI, "CHAN_RF", validRF);
			if (!validRF)
			{
				rfCentre = getKeywordByID<CORBA::Long>(tmp->SRI, "COL_RF", validRF);
			}
			if (validRF)
			{
				double ifCentre=0;
				if (tmp->SRI.mode==0) //real data is at fs/4.0
					ifCentre = 1.0/xdelta/4.0;
				double deltaF = rfCentre-ifCentre; //Translation between rf & if
				tmp->SRI.xstart = ifStart+deltaF;  //This the the start bin at RF
			}
			else
			{
				LOG_WARN(psd_i, "rf Frequency units requested but no rf unit keyword present");
			}
		}
		if (!validRF)
			tmp->SRI.xstart = ifStart;

		if (tmp->SRI.mode==0)
			tmp->SRI.subsize = params.fftSz/2+1;
		else
			tmp->SRI.subsize =params.fftSz;
		tmp->SRI.ydelta = xdelta*params.strideSize;
		tmp->SRI.yunits = BULKIO::UNITS_TIME;
		tmp->SRI.xunits = BULKIO::UNITS_FREQUENCY;
		tmp->SRI.mode = 1; //data is always complex out of the fft
		fft_dataFloat_out->pushSRI(tmp->SRI);
		if (numAvg > 2)
			tmp->SRI.ydelta*=params.numAverage;
		tmp->SRI.mode = 0; //data is always real out of the psd
		psd_dataFloat_out->pushSRI(tmp->SRI);
	}

	//to do - should adjust T for extra sample delay from elements in last loop
	if (!psdOutVec_.empty())
	{
		psd_dataFloat_out->pushPacket(psdOutVec_, tmp->T, tmp->EOS, tmp->streamID);
	}
	if (!fftOutVec_.empty())
	{
		fft_dataFloat_out->pushPacket(fftOutVec_, tmp->T, tmp->EOS, tmp->streamID);
	}
	delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
	return NORMAL;
}

void psd_i::fftSizeChanged(const unsigned int *oldValue, const unsigned int *newValue)
{
	if (*oldValue != *newValue) {
		boost::mutex::scoped_lock lock(psdLock_);
		for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++) {
			i->second->updateFftSize(fftSize);
		}
	}
}

void psd_i::numAvgChanged(const unsigned int *oldValue, const unsigned int *newValue)
{
	if (*oldValue != *newValue) {
		boost::mutex::scoped_lock lock(psdLock_);
		for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
			i->second->updateNumAvg(numAvg);
	}
}

void psd_i::overlapChanged(const int *oldValue, const int *newValue)
{
	if (*oldValue != *newValue) {
		boost::mutex::scoped_lock lock(psdLock_);
		for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
			i->second->updateOverlap(overlap);
	}
}

void psd_i::callBackFunc( const char* connectionId)
{
	doPSD = (psd_dataFloat_out->state()!=BULKIO::IDLE);
	doFFT = (fft_dataFloat_out->state()!=BULKIO::IDLE);
}

void psd_i::rfFreqUnitsChanged(const bool *oldValue, const bool *newValue)
{
	if (*oldValue != *newValue) {
		boost::mutex::scoped_lock lock(psdLock_);
		for (map_type::iterator i = stateMap.begin(); i!=stateMap.end(); i++)
			i->second->forceSRIUpdate();
	}
}
