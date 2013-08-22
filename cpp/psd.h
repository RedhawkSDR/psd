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
		template <typename TimeType>
		void serviceLoop(Fft<TimeType>* psd, TimeType& psdInput, std::vector<float>& psdOutVec, std::vector<float>& fftOutVec, std::vector< framebuffer<std::vector<float>::iterator>::frame> & input);

		framebuffer<std::vector<float>::iterator> frameBuffer_;

		RealPsd* realPsd_;
		ComplexPsd* complexPsd_;
		VectorMean<float, fftwf_allocator<float> > vecMean_;
		RealFFTWVector realIn_;
		ComplexFFTWVector complexIn_;
		ComplexFFTWVector fftOut_;
		RealFFTWVector psdOut_;
		std::vector<float> psdOutput_;
		std::vector<float> psdAverage_;
		boost::mutex psdLock_;
		bool updateSRI_;
		bool doPSD;
		bool doFFT;

        bulkio::MemberConnectionEventListener<psd_i> listener;
        void callBackFunc( const char* connectionId);
};

#endif
