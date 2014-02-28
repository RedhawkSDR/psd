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

class psd_i;

class PsdProcessor
{
	//class to take care of psd procesing
	//hanldes real/complex with transitions and
	//output averaging and overlap and buffering and db conversion
	//basically - you give it time domain data and it gives you frequency domain
	//
	//this class does both fft,psd, or both (or neither) as requested at processigng time
public:
	struct ParamStruct {
		size_t fftSz;
		bool updateSRI;
		size_t strideSize;
		size_t numAverage;
	};
	PsdProcessor(std::vector<float>& psdOutVec,
			 std::vector<float>& fftOutVec,
			 size_t fftSize,
			 int overlap,
			 size_t numAvg);
	~PsdProcessor();

	void updateFftSize(size_t fftSize);
	void updateOverlap(int overlap);
	void updateNumAvg(size_t avg);
	void flush();
	ParamStruct process(std::vector<float>& input, bool cmplx, bool doPSD, bool doFFT, float logCoeficient);

private:
	//for output
	std::vector<float>& psdOutVec_;
	std::vector<float>& fftOutVec_;

	template <typename TimeType>
	void serviceLoop(Fft<TimeType>* psd, TimeType& psdInput, bool doPSD, bool doFFT, float logCoeficient);

	framebuffer<std::vector<float>::iterator> frameBuffer_;
	std::vector<framebuffer<std::vector<float>::iterator>::frame> framedData_;

	RealPsd* realPsd_;
	ComplexPsd* complexPsd_;
	//internal processing vectors

	VectorMean<float, fftwf_allocator<float> > vecMean_;
	RealFFTWVector realIn_;
	ComplexFFTWVector complexIn_;
	ComplexFFTWVector fftOut_;
	RealFFTWVector psdOut_;

	//internal vector for psd averaging
	std::vector<float> psdAverage_;

	int overlap_;
	ParamStruct params_;

};

class psd_i : public psd_base
{
    ENABLE_LOGGING
    public:
        psd_i(const char *uuid, const char *label);
        ~psd_i();
        int serviceFunction();
	private:
		void fftSizeChanged(const std::string& id);
		void overlapChanged(const std::string& id);
		void numAvgChanged(const std::string& id);

		typedef std::map<std::string, PsdProcessor*> map_type;
		map_type stateMap;

		std::vector<float> psdOutVec_;
		std::vector<float> fftOutVec_;

		boost::mutex psdLock_;

		bool doPSD;
		bool doFFT;

        bulkio::MemberConnectionEventListener<psd_i> listener;
        void callBackFunc( const char* connectionId);
};

#endif
