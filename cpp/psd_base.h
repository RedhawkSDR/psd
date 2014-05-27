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
#ifndef PSD_IMPL_BASE_H
#define PSD_IMPL_BASE_H

#include <boost/thread.hpp>
#include <ossie/Resource_impl.h>
#include <ossie/ThreadedComponent.h>

#include <bulkio/bulkio.h>

class psd_base : public Resource_impl, protected ThreadedComponent
{
    public:
        psd_base(const char *uuid, const char *label);
        ~psd_base();

        void start() throw (CF::Resource::StartError, CORBA::SystemException);

        void stop() throw (CF::Resource::StopError, CORBA::SystemException);

        void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

        void loadProperties();

    protected:
        // Member variables exposed as properties
        CORBA::ULong fftSize;
        CORBA::Long overlap;
        CORBA::ULong numAvg;
        float logCoefficient;

        // Ports
        bulkio::InFloatPort *dataFloat_in;
        bulkio::OutFloatPort *psd_dataFloat_out;
        bulkio::OutFloatPort *fft_dataFloat_out;

    private:
};
#endif // PSD_IMPL_BASE_H
