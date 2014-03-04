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

class ComponentTests(ossie.utils.testing.ScaComponentTestCase):
    """Test for all component implementations in psd"""

    """To do BETTER UNIT TESTS NEEDED FOR THIS COMPONENT
    """

    def testScaBasicBehavior(self):
        #######################################################################
        # Launch the component with the default execparams
        execparams = self.getPropertySet(kinds=("execparam",), modes=("readwrite", "writeonly"), includeNil=False)
        execparams = dict([(x.id, any.from_any(x.value)) for x in execparams])
        self.launch(execparams, initialize=True)
        
        #######################################################################
        # Verify the basic state of the component
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
            
        self.src = sb.DataSource()
        self.sink = sb.DataSink()
            
        #######################################################################
        # Make sure start and stop can be called without throwing exceptions
        self.comp.start()
         
        self.src.start()
        self.sink.start()
        
        self.src.connect(self.comp)
        self.comp.connect(self.sink, usesPortName='psd_dataFloat_out')
        
        inData = [float(0), float(0), float(0), float(1.0)] *(100000)
        numLoop = 4
        overlap = 4
        frameLength = 5
        numStride = len(inData)/numLoop
        print "numStride = ", numStride

        eos=False
        for i in xrange(numLoop):
            time.sleep(.01)           
            if i==numLoop-1:
                eos=True
            self.src.push(inData[i*numStride:(i+1)*numStride], complexData=True, EOS=eos)
        
        time.sleep(1.5)
        
        
        newOut = self.sink.getData()
        print "got %s elements out" %len(newOut)
        
        print max(newOut)
        print min(newOut)
        
        time.sleep(.5)
        
        self.comp.stop()

        #######################################################################
        # Simulate regular component shutdown
        self.comp.releaseObject()
        
    # TODO Add additional tests here
    #
    # See:
    #   ossie.utils.bulkio.bulkio_helpers,
    #   ossie.utils.bluefile.bluefile_helpers
    # for modules that will assist with testing components with BULKIO ports
    
if __name__ == "__main__":
    ossie.utils.testing.main("../psd.spd.xml") # By default tests all implementations
