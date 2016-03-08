#!/usr/bin/env python
# This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this 
# source distribution.
# 
# This file is part of REDHAWK Basic Components psd.
# 
# REDHAWK Basic Components psd is free software: you can redistribute it and/or modify it under the terms of 
# the GNU General Public License as published by the Free Software Foundation, either 
# version 3 of the License, or (at your option) any later version.
# 
# REDHAWK Basic Components psd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
# PURPOSE.  See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along with this 
# program.  If not, see http://www.gnu.org/licenses/.
#

import unittest
import ossie.utils.testing
import os
from omniORB import any
import time
from ossie.utils import sb
try:
    from pylab import figure, plot, grid, show, title
except ImportError:
    print "no pylab for you"
    figure=None
from numpy import cos, sin, arange, pi, correlate, linspace
import scipy.fftpack
import numpy as np
import types
import random

DEBUG_LEVEL=3

PRECISION=6
NUM_PLACES=6

def isclose(a, b, rel_tol=1e-09, abs_tol=0.0,debug=False):
    ''' Return True if the values a and b are close to each other and False otherwise.

        Whether or not two values are considered close is determined according to given absolute and relative tolerances.

        rel_tol is the relative tolerance - it is the maximum allowed difference between a and b, relative to the larger
        absolute value of a or b. For example, to set a tolerance of 5%, pass rel_tol=0.05. The default tolerance is 1e-09,
        which assures that the two values are the same within about 9 decimal digits. rel_tol must be greater than zero.

        abs_tol is the minimum absolute tolerance - useful for comparisons near zero. abs_tol must be at least zero.
        
        Note: take from math module in newer versions of Python (>= 3.5)
    '''
    retval = abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)
    if debug:
        _abs=abs(a-b)
        _tol=max(rel_tol * max(abs(a), abs(b)), abs_tol)
        _rel=rel_tol * max(abs(a), abs(b))
        print '.' if retval else 'F', repr(a), repr(b), repr(_abs), repr(_tol), repr(_rel), repr(abs_tol)

    return retval
    #return abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)

def packCx(data):
    real=None
    out=[]
    for val in data:
        if real==None:
            real=val
        else:
            out.append(abs(complex(real,val)))
            real=None
    return out

def unpackCx(data):
    out = []
    for val in data:
        out.append(float(val.real))
        out.append(float(val.imag))
    return out

def plotFreqData(fftSize, sampleRate, pyFFT, fftOut, psdOut):
    if figure:
        freqs = linspace(0,1,fftSize/2) * (sampleRate/2)
            
        figure(1)
        title("REAL Py fft")
        plot(freqs, pyFFT)
           
        figure(2)
        title("REAL real fft")
        plot(freqs, fftOut)
           
        figure(3)
        title("REAL comp psd")
        plot(freqs, psdOut)
            
        grid(True)
        show(True)
    
class ComponentTests(ossie.utils.testing.ScaComponentTestCase):
    """Test for all component implementations in psd"""
    
    def setUp(self):
        """Set up the unit test - this is run before every method that starts with test"""
        ossie.utils.testing.ScaComponentTestCase.setUp(self)
        self.src = sb.DataSource()
        self.fftsink = sb.DataSink()
        self.psdsink = sb.DataSink()
        self.comp = sb.launch('../psd.spd.xml',execparams={'DEBUG_LEVEL':DEBUG_LEVEL})
        
        self.src.connect(self.comp)
        self.comp.connect(self.fftsink, usesPortName='fft_dataFloat_out')
        self.comp.connect(self.psdsink, usesPortName='psd_dataFloat_out')
        
    def tearDown(self):
        """Finish the unit test - this is run after every method that starts with test"""
        sb.stop()
        self.comp.releaseObject()
        ossie.utils.testing.ScaComponentTestCase.tearDown(self)
        
    def validateSRIPushing(self, streamID, inputCmplx, inputRate=1.0, fftSize=1.0, rfVal = None, SRIKeywords = None):

        # Confirm that all of the keywords given are passed on
        if SRIKeywords:
          self.assertEqual(len(SRIKeywords), len(self.psdsink.sri().keywords), "The provided SRI Keyword length does not equal what was received by the PSD output SRI")
          self.assertEqual(len(SRIKeywords), len(self.fftsink.sri().keywords), "The provided SRI Keyword length does not equal what was received by the FFT output SRI")
  
          for psd_keyword in self.psdsink.sri().keywords:
            found_match = False
            for given_keyword in SRIKeywords:
              if given_keyword._name == psd_keyword.id and given_keyword._value == psd_keyword.value.value():
                print "Found match"
                found_match = True
            self.assertTrue(found_match, "Not all keywords in source matched output of PSD port")


        xdelta = inputRate/fftSize

        for sri in (self.fftsink.sri(), self.psdsink.sri()):
            self.assertEqual(sri.streamID, streamID)
            self.assertAlmostEqual(sri.xdelta, xdelta)
            if inputCmplx==1:
                ifStart = -xdelta*(fftSize/2-1)
            else:
                ifStart=0
            if rfVal !=None and self.comp.rfFreqUnits:
                if inputCmplx:
                    self.assertAlmostEqual(sri.xstart, ifStart+rfVal)
                else:
                    self.assertAlmostEqual(sri.xstart, rfVal-inputRate/4.0)
            else:
                self.assertAlmostEqual(sri.xstart, ifStart)
        
    def assert_isclose(self,a,b,precision=7,places=7):
        ''' Similar to assertAlmostEqual, but based on digits of precision rather than decimal places
            For 32-bit systems (float), the greatest precision possible is 7 digits.
            For 64-bit systems (double), the greatest precision possible is 15 digits.
            Set places for numbers near 0, where relative precision may fail.
        '''
        rel_tol=10**(-1*precision)
        abs_tol=10**(-1*places)
        # Rather than reimplement assert, call assertAlmostEqual which will fail
        isclose(a,b,rel_tol,abs_tol) or self.assertAlmostEqual(a,b,places)

    def testScaBasicBehavior(self):
        print "\n-------- TESTING Basic Behavior --------"
        #######################################################################
        # Launch the resource with the default execparams
        execparams = self.getPropertySet(kinds=("execparam",), modes=("readwrite", "writeonly"), includeNil=False)
        execparams = dict([(x.id, any.from_any(x.value)) for x in execparams])
        self.launch(execparams)

        #######################################################################
        # Verify the basic state of the resource
        self.assertNotEqual(self.comp, None)
        self.assertEqual(self.comp.ref._non_existent(), False)

        self.assertEqual(self.comp.ref._is_a("IDL:CF/Resource:1.0"), True)

        #######################################################################
        # Validate that query returns all expected parameters
        # Query of '[]' should return the following set of properties
        expectedProps = []
        expectedProps.extend(self.getPropertySet(kinds=("configure", "execparam"), modes=("readwrite", "readonly"), includeNil=True))
        expectedProps.extend(self.getPropertySet(kinds=("allocate",), action="external", includeNil=True))
        props = self.comp.query([])
        props = dict((x.id, any.from_any(x.value)) for x in props)
        # Query may return more than expected, but not less
        for expectedProp in expectedProps:
            self.assertEquals(props.has_key(expectedProp.id), True)

        #######################################################################
        # Verify that all expected ports are available
        for port in self.scd.get_componentfeatures().get_ports().get_uses():
            port_obj = self.comp.getPort(str(port.get_usesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a("IDL:CF/Port:1.0"),  True)

        for port in self.scd.get_componentfeatures().get_ports().get_provides():
            port_obj = self.comp.getPort(str(port.get_providesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a(port.get_repid()),  True)

        #######################################################################
        # Make sure start and stop can be called without throwing exceptions
        self.comp.start()
        self.comp.stop()

        #######################################################################
        # Simulate regular resource shutdown
        self.comp.releaseObject()
        
        print "*PASSED"
        
    def testRealData1(self):
        print "\n-------- TESTING w/REAL DATA1 --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "RealData1"
        fftSize = 4096
        self.comp.fftSize = fftSize
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096.
    
        F_7KHz = 7000.
        A_7KHz = 5.0

        t = arange(nsamples) / sample_rate
        tmpData = A_7KHz * cos(2*pi*F_7KHz*t)

        data = []
        [data.append(float(x)) for x in tmpData]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = False
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize)
        
        #Convert Redhawk interleaved complex data to python complex for fftOut
        fftOut = packCx(fftOut)
        
        # Adjust length of data for accurate comparisons
        pyFFT = pyFFT[0:fftSize/2]
        fftOut = fftOut[0:fftSize/2]
        psdOut = psdOut[0:fftSize/2]

        # Uncomment function below to see output plots:
        #plotFreqData(fftSize, sample_rate, pyFFT, fftOut, psdOut)
        
        # Find Max values and their corresponding frequency index
        pyFFTMax = max(pyFFT)
        fftOutMax = max(fftOut)
        psdOutMax = max(psdOut)
        pyMaxSquared = pyFFTMax**2
        
        pyFFTIndex = pyFFT.tolist().index(pyFFTMax)
        fftIndex = fftOut.index(fftOutMax)
        psdIndex = psdOut.index(psdOutMax)
        
        # Check that the component max values and index values are equal to python's
        threshold = 0.000001
        self.assertFalse(abs(pyFFTMax-fftOutMax) >= fftOutMax*threshold)
        self.assertFalse(abs(pyMaxSquared-psdOutMax) >= psdOutMax*threshold)
        self.assertFalse(abs(pyFFTIndex-fftIndex) >= 1.0)
        self.assertFalse(abs(pyFFTIndex-psdIndex) >= 1.0)
        
        print "*PASSED"
        
    def testRealData2(self):
        print "\n-------- TESTING w/REAL DATA2 --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "RealData2"
        fftSize = 8192
        self.comp.fftSize = fftSize
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 8192 samples of (3000Hz + 7000Hz) real signal at 32768 kHz
        sample_rate = 32768.
        nsamples = 8192.
    
        F_3KHz = 3000.
        A_3KHz = 10.0
    
        F_7KHz = 7000.
        A_7KHz = 5.0

        t = arange(nsamples) / sample_rate
        tmpData = A_7KHz * cos(2*pi*F_7KHz*t) + A_3KHz * cos(2*pi*F_3KHz*t)

        data = []
        [data.append(float(x)) for x in tmpData]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData=False
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        pyFFT = abs(scipy.fft(tmpData, fftSize))
        
        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize)

        #Convert Redhawk interleaved complex data to python complex for fftOut
        fftOut = packCx(fftOut)

        # Adjust length of data for accurate comparisons
        pyFFT = pyFFT[0:fftSize/2]
        fftOut = fftOut[0:fftSize/2]
        psdOut = psdOut[0:fftSize/2]

        # Uncomment function below to see output plots:
        #plotFreqData(fftSize, sample_rate, pyFFT, fftOut, psdOut)
        
        # Find Max values and their corresponding frequency index
        pyFFTMax = max(pyFFT)
        fftOutMax = max(fftOut)
        psdOutMax = max(psdOut)
        pyMaxSquared = pyFFTMax**2
        
        pyFFTIndex = pyFFT.tolist().index(pyFFTMax)
        fftIndex = fftOut.index(fftOutMax)
        psdIndex = psdOut.index(psdOutMax)
        
        # Check that the component max values and index values are equal to python's
        threshold = 0.000001
        self.assertFalse(abs(pyFFTMax-fftOutMax) >= fftOutMax*threshold)
        self.assertFalse(abs(pyMaxSquared-psdOutMax) >= psdOutMax*threshold)
        self.assertFalse(abs(pyFFTIndex-fftIndex) >= 1.0)
        self.assertFalse(abs(pyFFTIndex-psdIndex) >= 1.0)

        print "*PASSED"

    def testComplexData1(self):
        print "\n-------- TESTING w/COMPLEX DATA1 --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "ComplexData1"
        fftSize = 8192
        self.comp.fftSize = fftSize
        
        #------------------------------------------------
        # Create test signal
        #------------------------------------------------
        # 8192 samples of (3000Hz + 7000Hz) complex signal at 32768 kHz
        sample_rate = 32768.
        nsamples = 8192.
    
        F_3KHz = 3000.
        A_3KHz = 10.0
    
        F_7KHz = 7000.
        A_7KHz = 5.0

        t = arange(nsamples) / sample_rate
        tmpData = A_7KHz * np.exp(1j*2*pi*F_7KHz*t) + A_3KHz * np.exp(1j*2*pi*F_3KHz*t)

        #Convert signal from python complex to RedHawk interleaved complex
        data = unpackCx(tmpData)
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData=True
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        pyFFT = abs(scipy.fft(tmpData, fftSize))
 
        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize)
        
        #Convert Redhawk interleaved complex data to python complex for fftOut
        fftOut = packCx(fftOut)
        
        # Adjust length of data for accurate comparisons
        pyFFT = pyFFT[0:fftSize/2]
        psdOut = psdOut[fftSize/2:fftSize]
        fftOut = fftOut[fftSize/2:fftSize]
        
        # Uncomment function below to see output plots:
        #plotFreqData(fftSize, sample_rate, pyFFT, fftOut, psdOut)
        
        # Normalize the data for accurate comparison with python fft
        pyFFTMax = max(pyFFT)
        fftOutMax = max(fftOut)
        psdOutMax = max(psdOut)
        pyMaxSquared = pyFFTMax**2
        
        pyFFTIndex = pyFFT.tolist().index(pyFFTMax)
        fftIndex = fftOut.index(fftOutMax)
        psdIndex = psdOut.index(psdOutMax)
        
        # Check that the component max values and index values are equal to python's
        threshold = 0.000001
        self.assertFalse(abs(pyFFTMax-fftOutMax) >= fftOutMax*threshold)
        self.assertFalse(abs(pyMaxSquared-psdOutMax) >= psdOutMax*threshold)
        self.assertFalse(abs(pyFFTIndex-fftIndex) >= 1.0)
        self.assertFalse(abs(pyFFTIndex-psdIndex) >= 1.0)
        
        print "*PASSED"

    def testComplexData2(self):
        print "\n-------- TESTING w/COMPLEX DATA2 --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "ComplexData2"
        fftSize = 4096
        self.comp.fftSize = fftSize
        
        #------------------------------------------------
        # Create test signal
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096.
    
        F_7KHz = 7000.
        A_7KHz = 5.0
        
        t = arange(nsamples) / sample_rate
        tmpData = A_7KHz * np.exp(1j*2*pi*F_7KHz*t)

        #Convert signal from python complex to RedHawk interleaved complex
        data = unpackCx(tmpData)
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData=True
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        pyFFT = abs(scipy.fft(tmpData, fftSize))
 
        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize)
        
        #Convert Redhawk interleaved complex data to python complex for fftOut
        fftOut = packCx(fftOut)
        
        # Adjust length of data for accurate comparisons
        pyFFT = pyFFT[0:fftSize/2]
        psdOut = psdOut[fftSize/2:fftSize]
        fftOut = fftOut[fftSize/2:fftSize]

        # Uncomment function below to see output plots:
        #plotFreqData(fftSize, sample_rate, pyFFT, fftOut, psdOut)
        
        # Normalize the data for accurate comparison with python fft
        pyFFTMax = max(pyFFT)
        fftOutMax = max(fftOut)
        psdOutMax = max(psdOut)
        pyMaxSquared = pyFFTMax**2
        
        pyFFTIndex = pyFFT.tolist().index(pyFFTMax)
        fftIndex = fftOut.index(fftOutMax)
        psdIndex = psdOut.index(psdOutMax)

        # additional checks due to floating point precision
        # look at value before peak to see if w/in tolerance
        # if so, use that as peak index to report the FIRST peak value
        rel_tol=10**(-1*PRECISION)
        abs_tol=10**(-1*NUM_PLACES)
        if isclose(pyFFT[pyFFTIndex-1], pyFFT[pyFFTIndex], rel_tol, abs_tol):
            pyFFTIndex = pyFFTIndex-1
        if isclose(fftOut[fftIndex-1], fftOut[fftIndex], rel_tol, abs_tol):
            fftIndex = fftIndex-1
        if isclose(psdOut[psdIndex-1], psdOut[psdIndex], rel_tol, abs_tol):
            psdIndex = psdIndex-1

        # Check that the component max values and index values are equal to python's
        self.assert_isclose(pyFFTMax,fftOutMax,PRECISION,NUM_PLACES)
        self.assert_isclose(pyMaxSquared,psdOutMax,PRECISION,NUM_PLACES)
        self.assertEqual(pyFFTIndex,fftIndex)
        self.assertEqual(pyFFTIndex,psdIndex)
        
        print "*PASSED"

    def testColRfReal(self):
        print "\n-------- TESTING w/REAL ColRf --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "ColRfReal"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  True
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = False
        colRfVal = 100e6
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'float')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)


        print "*PASSED"

    def testColRfCx(self):
        print "\n-------- TESTING w/COMPLEX ColRf --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "ColRfCx"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  True
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        colRfVal = 100e6
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'float')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)


        print "*PASSED"
        
    def testColRfCxToggle(self):
        print "\n-------- TESTING w/COMPLEX rfFreqUnitsToggle COL_RF --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "rfFreqUnitsToggleColRf"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  False
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        colRfVal = 100e6
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'float')]
        #keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'double')]
        #keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'long')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)

        self.comp.rfFreqUnits =  True
        time.sleep(.5)
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)
        
        print "*PASSED"
        
    def testChanRfCxToggle(self):
        print "\n-------- TESTING w/COMPLEX rfFreqUnitsToggle CHAN_RF--------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "rfFreqUnitsToggleChanRf"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  False
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        chanRfVal = 100e6
        #keywords = [sb.io_helpers.SRIKeyword('CHAN_RF',chanRfVal, 'float')]
        keywords = [sb.io_helpers.SRIKeyword('CHAN_RF',chanRfVal, 'double')]
        #keywords = [sb.io_helpers.SRIKeyword('CHAN_RF',chanRfVal, 'long')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, chanRfVal)

        self.comp.rfFreqUnits =  True
        time.sleep(.5)
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, chanRfVal, SRIKeywords = keywords)
        
        print "*PASSED"
        
    def testChanRfCxTogglePartial(self):
        print "\n-------- TESTING w/COMPLEX rfFreqUnitsTogglePartial CHAN_RF--------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "rfFreqUnitsTogglePartialChanRf"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  False
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        chanRfVal = 100e6
        #keywords = [sb.io_helpers.SRIKeyword('CHAN_RF',chanRfVal, 'float')]
        keywords = [sb.io_helpers.SRIKeyword('CHAN_RF',chanRfVal, 'double')]
        #keywords = [sb.io_helpers.SRIKeyword('CHAN_RF',chanRfVal, 'long')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, chanRfVal, SRIKeywords = keywords)
        
        # Push partial block
        time.sleep(.5)
        self.src.push(data[:nsamples], streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)
        
        # Toggle property and push rest of partial block
        self.comp.rfFreqUnits =  True
        time.sleep(.5)
        self.src.push(data[nsamples:], streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, chanRfVal, SRIKeywords = keywords)
        
        print "*PASSED"
        
    def testEosFull(self):
        print "\n-------- TESTING EOS w/COMPLEX DATA FULL --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "eosFull"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  False
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        colRfVal = 100e6
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'float')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)

        time.sleep(.5)
        self.src.push(data, EOS=True, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        self.assertTrue(self.fftsink.eos())
        self.assertTrue(self.psdsink.eos())
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)
        
        print "*PASSED"
        
    def testEosEmpty(self):
        print "\n-------- TESTING EOS w/COMPLEX DATA EMPTY --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "eosEmpty"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  False
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        colRfVal = 100e6
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'float')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)

        time.sleep(.5)
        self.src.push([], EOS=True, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        print 'pushed %s samples w/ EOS=true, fftSize=%s'%(0,fftSize)
        time.sleep(.5)

        # Get Output Data
        self.assertTrue(self.fftsink.eos())
        self.assertTrue(self.psdsink.eos())
        fftOut = self.fftsink.getData() # should be empty
        psdOut = self.psdsink.getData() # should be empty
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)
        
        print "*PASSED"
        
    def testEosPartial(self):
        print "\n-------- TESTING EOS w/COMPLEX DATA PARTIAL --------"
        #---------------------------------
        # Start component and set fftSize
        #---------------------------------
        sb.start()
        ID = "eosPartial"
        fftSize = 4096
        self.comp.fftSize = fftSize
        self.comp.rfFreqUnits =  False
        
        #------------------------------------------------
        # Create a test signal.
        #------------------------------------------------
        # 4096 samples of 7000Hz real signal at 65536 kHz
        sample_rate = 65536.
        nsamples = 4096

        data = [random.random() for _ in xrange(2*nsamples)]
        
        #------------------------------------------------
        # Test Component Functionality.
        #------------------------------------------------
        # Push Data
        cxData = True
        colRfVal = 100e6
        keywords = [sb.io_helpers.SRIKeyword('COL_RF',colRfVal, 'float')]
        self.src.push(data, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        time.sleep(.5)

        # Get Output Data
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        #pyFFT = abs(scipy.fft(tmpData, fftSize))

        #Validate SRI Pushed Correctly
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)

        time.sleep(.5)
        self.src.push(data[:fftSize], EOS=True, streamID=ID, sampleRate=sample_rate, complexData=cxData, SRIKeywords = keywords)
        print 'pushed %s samples w/ EOS=true, fftSize=%s'%(len(data[:fftSize]),fftSize)
        time.sleep(.5)

        # Get Output Data
        self.assertTrue(self.fftsink.eos())
        self.assertTrue(self.psdsink.eos())
        fftOut = self.fftsink.getData()[0] # use first frame
        psdOut = self.psdsink.getData()[0] # use first frame
        self.validateSRIPushing(ID, cxData, sample_rate, fftSize, colRfVal, SRIKeywords = keywords)
        
        print "*PASSED"
    
if __name__ == "__main__":
    ossie.utils.testing.main("../psd.spd.xml") # By default tests all implementations
