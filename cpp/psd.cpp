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

psd_i::psd_i(const char *uuid, const char *label) :
   psd_base(uuid, label),
   frameBuffer_(fftSize,0),
   realPsd_(new RealPsd(realIn_, psdOut_, fftOut_, fftSize,true)),
   complexPsd_(NULL),
   vecMean_(numAvg, psdOut_, psdAverage_),
   updateSRI_(false)
{
	setPropertyChangeListener("fftSize", this, &psd_i::fftSizeChanged);
	setPropertyChangeListener("overlap", this, &psd_i::overlapChanged);
	setPropertyChangeListener("numAvg", this, &psd_i::numAvgChanged);
	fftSizeChanged("");
	frameBuffer_.setOverlap(overlap);

}

psd_i::~psd_i()
{
	if (realPsd_)
	{
		delete realPsd_;
		realPsd_ = NULL;
	}
	if (complexPsd_)
	{
		delete complexPsd_;
		complexPsd_=NULL;
	}
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

	std::vector<framebuffer<std::vector<float>::iterator>::frame> framedData;
	std::vector<float> inVec;
	std::vector<float> psdOutVec;
	std::vector<float> fftOutVec;
	inVec.assign(tmp->dataBuffer.begin(), tmp->dataBuffer.end());

	{
		boost::mutex::scoped_lock lock(psdLock_);

		if (tmp->SRI.mode==1 && realPsd_)
		{
			std::cout<<"switch to complex"<<std::endl;
			delete realPsd_;
			realPsd_=NULL;
			complexPsd_ = new ComplexPsd(complexIn_, psdOut_, fftOut_, fftSize,true);
			frameBuffer_.setFrameSize(fftSize*2);
			frameBuffer_.setOverlap(2*overlap);

		} else if (tmp->SRI.mode==0 && complexPsd_)
		{
			std::cout<<"switch to real"<<std::endl;
			delete complexPsd_;
			complexPsd_=NULL;
			realPsd_ = new RealPsd(realIn_, psdOut_, fftOut_, fftSize,true);
			frameBuffer_.setFrameSize(fftSize);
			frameBuffer_.setOverlap(overlap);
		}

		frameBuffer_.newData(inVec,framedData);

		if (complexPsd_)
		{
			serviceLoop(complexPsd_, complexIn_, psdOutVec, fftOutVec, framedData);
		}
		else if (realPsd_)
		{
			serviceLoop(realPsd_, realIn_, psdOutVec, fftOutVec, framedData);
		}
		else
		{
			std::cerr<<"this shoudln't happen - no real or complex psd created"<<std::endl;
		}
	}
	// NOTE: You must make at least one valid pushSRI call
	if (tmp->sriChanged || updateSRI_) {
		double xdelta = tmp->SRI.xdelta;
		if (tmp->SRI.mode==0)
			tmp->SRI.subsize = fftSize/2+1;
		else
			tmp->SRI.subsize =fftSize;
		tmp->SRI.ydelta = xdelta*(fftSize-overlap);
		tmp->SRI.xdelta = 1.0/(xdelta*fftSize);
		if (tmp->SRI.mode==0)
			tmp->SRI.xstart=0;
		else
			tmp->SRI.xstart=-((fftSize/2-1)*tmp->SRI.xdelta);
		tmp->SRI.yunits = BULKIO::UNITS_FREQUENCY;
		tmp->SRI.mode = 1; //data is always complex out of the fft
		fft_dataFloat_out->pushSRI(tmp->SRI);
		if (numAvg > 2)
			tmp->SRI.ydelta*=numAvg;
		tmp->SRI.mode = 0; //data is always real out of the psd
		psd_dataFloat_out->pushSRI(tmp->SRI);
	}

	//to do - should adjust T for extra sample delay from elements in last loop
	if (!psdOutVec.empty())
	{
		psd_dataFloat_out->pushPacket(psdOutVec, tmp->T, tmp->EOS, tmp->streamID);
	}
	if (!fftOutVec.empty())
	{
		fft_dataFloat_out->pushPacket(fftOutVec, tmp->T, tmp->EOS, tmp->streamID);
	}
	delete tmp; // IMPORTANT: MUST RELEASE THE RECEIVED DATA BLOCK
	return NORMAL;
}

template <typename TimeType>
void psd_i::serviceLoop(Fft<TimeType>* psd, TimeType& psdInput, std::vector<float>& psdOutVec, std::vector<float>& fftOutVec, std::vector< framebuffer<std::vector<float>::iterator>::frame> & input)
{
	if (!input.empty())
	{
		bool doPSD = (psd_dataFloat_out->state()!=BULKIO::IDLE);
		bool doFFT = (fft_dataFloat_out->state()!=BULKIO::IDLE);
		for (unsigned int i=0; i!=input.size(); i++)
		{
			copyVec(input[i].begin,input[i].end,psdInput);
			psd->run();
			if (doPSD)
			{
				if (numAvg > 1)
				{
					if (vecMean_.run())
						appendVec(psdAverage_,psdOutVec);
				}
				else
					appendVec(psdOut_,psdOutVec);
			}
			if (doFFT)
				appendVec(fftOut_,fftOutVec);
		}
		if (!psdOutVec.empty() && logCoeficient >0)
		{
			//take the log of the output if necessary
			for (std::vector<float>::iterator i=psdOutVec.begin(); i!=psdOutVec.end(); i++)
				*i=logCoeficient*log10(*i);
		}
	}
}

void psd_i::fftSizeChanged(const std::string& id)
{
	boost::mutex::scoped_lock lock(psdLock_);
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
	updateSRI_=true;
}

void psd_i::overlapChanged(const std::string& id)
{
	boost::mutex::scoped_lock lock(psdLock_);
	long floatOverlap = overlap;
	if (complexPsd_)
	{
		floatOverlap=2*overlap;
	}
	frameBuffer_.setOverlap(floatOverlap);
	updateSRI_=true;
}

void psd_i::numAvgChanged(const std::string& id)
{
	boost::mutex::scoped_lock lock(psdLock_);
	vecMean_.setAvgNum(numAvg);
	updateSRI_=true;
}
