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

#include "psd_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/

psd_base::psd_base(const char *uuid, const char *label) :
    Resource_impl(uuid, label),
    ThreadedComponent()
{
    loadProperties();

    dataFloat_in = new bulkio::InFloatPort("dataFloat_in");
    addPort("dataFloat_in", dataFloat_in);
    psd_dataFloat_out = new bulkio::OutFloatPort("psd_dataFloat_out");
    addPort("psd_dataFloat_out", psd_dataFloat_out);
    fft_dataFloat_out = new bulkio::OutFloatPort("fft_dataFloat_out");
    addPort("fft_dataFloat_out", fft_dataFloat_out);
}

psd_base::~psd_base()
{
    delete dataFloat_in;
    dataFloat_in = 0;
    delete psd_dataFloat_out;
    psd_dataFloat_out = 0;
    delete fft_dataFloat_out;
    fft_dataFloat_out = 0;
}

/*******************************************************************************************
    Framework-level functions
    These functions are generally called by the framework to perform housekeeping.
*******************************************************************************************/
void psd_base::start() throw (CORBA::SystemException, CF::Resource::StartError)
{
    Resource_impl::start();
    ThreadedComponent::startThread();
}

void psd_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    Resource_impl::stop();
    if (!ThreadedComponent::stopThread()) {
        throw CF::Resource::StopError(CF::CF_NOTSET, "Processing thread did not die");
    }
}

void psd_base::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
    // This function clears the component running condition so main shuts down everything
    try {
        stop();
    } catch (CF::Resource::StopError& ex) {
        // TODO - this should probably be logged instead of ignored
    }

    Resource_impl::releaseObject();
}

void psd_base::loadProperties()
{
    addProperty(fftSize,
                32768,
                "fftSize",
                "",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(overlap,
                0,
                "overlap",
                "",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(numAvg,
                0,
                "numAvg",
                "",
                "readwrite",
                "",
                "external",
                "configure");

    addProperty(logCoefficient,
                0.0,
                "logCoefficient",
                "",
                "readwrite",
                "",
                "external",
                "configure");

}


