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

#ifndef PSD_IMPL_H
#define PSD_IMPL_H

#include "psd_base.h"
#include "fft.h"
#include "framebuffer.h"
#include "vectormean.h"


typedef struct ParamStruct {
	size_t fftSz;
	bool fftSzChanged;
	size_t strideSize;
	size_t numAverage;
	bool numAverageChanged;
	int overlap;
	bool doFFT;
	bool doPSD;
	bool rfFreqUnits;
	float logCoeff;
	bool updateSRI;
} param_struct;


class PsdProcessor : protected ThreadedComponent
{
    ENABLE_LOGGING
	//class to take care of psd processing
	//handles real/complex with transitions and
	//output averaging and overlap and buffering and db conversion
	//basically - you give it time domain data and it gives you frequency domain
	//
	//this class does both fft,psd, or both (or neither) as requested at processing time
public:
	PsdProcessor(bulkio::InFloatStream inStream, bulkio::OutFloatStream fftStream, bulkio::OutFloatStream psdStream,
			size_t fftSize, int overlap, size_t numAvg,	float logCoeff,	bool doFFT,	bool doPSD,	bool rfFreqUnits, float delay=0.1);
	~PsdProcessor();

	void updateFftSize(size_t fftSize);
	void updateOverlap(int overlap);
	void updateNumAvg(size_t avg);
	void updateRfFreqUnits(bool enable);
	void updateLogCoefficient(float logCoeff);
	void updateActions(bool psd, bool fft);
	void forceSRIUpdate();
	bool finished();
    void stop() throw (CF::Resource::StopError, CORBA::SystemException);

private:
	int serviceFunction(); // called by thread in a loop
	void updateSRI(const bulkio::FloatDataBlock &block);
	void flush();

	// in/out streams
	bulkio::InFloatStream in;
	bulkio::OutFloatStream outFFT;
	bulkio::OutFloatStream outPSD;

	// PSD processing object ptrs
	RealPsd* realPsd_;
	ComplexPsd* complexPsd_;

	//internal processing vectors
	RealFFTWVector realIn_;
	ComplexFFTWVector complexIn_;
	ComplexFFTWVector fftOut_;
	RealFFTWVector psdOut_;

	// for psd averaging
	VectorMean<float, fftwf_allocator<float> > vecMean_;
	std::vector<float> psdAverage_;

	//for output - TODO - can we do without this additional data structure/memcpy?
	std::vector<float> psdOutVec;
	std::vector<float> fftOutVec;

	// parameters and status
	bool eos;
	param_struct params;
	param_struct params_cache;
	boost::shared_ptr<boost::mutex> paramLock;
};

class psd_i : public psd_base
{
    ENABLE_LOGGING
    public:
        psd_i(const char *uuid, const char *label);
        ~psd_i();
        int serviceFunction();
        void stop() throw (CF::Resource::StopError, CORBA::SystemException);
        void streamAdded(bulkio::InFloatStream stream);
	private:
		void fftSizeChanged(unsigned int oldValue, unsigned int newValue);
		void numAvgChanged(unsigned int oldValue, unsigned int newValue);
		void overlapChanged(int oldValue, int newValue);
		void rfFreqUnitsChanged(bool oldValue, bool newValue);
		void logCoeffChanged(float oldValue, float newValue);
		void clearThreads();

		typedef std::map<std::string, boost::shared_ptr<PsdProcessor> > map_type;
		map_type stateMap;
		boost::mutex stateMapLock;

		bool doPSD;
		bool doFFT;

        bulkio::MemberConnectionEventListener<psd_i> listener;
        void callBackFunc( const char* connectionId);
};

#endif
