"""contains and controls mixer function data"""


import wx
import SubFunctionBlocks as FBlocksAPI
import subMAVFunctionSettings as MAVFSettingsAPI
import SubpyFEditSettings as FESettings
import SubpyFEditProject as FEProject
import pyCFiles as CFileGen

import struct, array

import sys,os
#import scanwin32

class callback_type(object):
# Status changes
    OFFLINE = 0
    ONLINE = 1
    SYNC_IN_PROGRESS = 2
    SYNC_COMPLETE = 4
    SYNC_FAIL = 6
    NV_MEM_WRITE_OK = 50
    NV_MEM_WRITE_FAIL = 55
    
#document changes
    FUNCTION_MODIFIED = 110
    FUNCTION_CHANGED = 112
    FUNCTIONS_CHANGED = 115
    REGISTERS_CHANGED = 130
    REGISTER_MODIFIED = 132
    NOT_SYNCHRONISED  = 150

# actions    
    UPDATE_ALL      = 225
    UPDATE_FUNCTION = 265
    

def PercentToQ14(percent):
    try:
        val = float(percent)
    except:
        val = 0.0;
    return (int) (val * 163.84)
    
# find the mavlink.py module
for d in [ 'pymavlink',
          os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', 'MAVLink', 'pymavlink') ]:
    if os.path.exists(d):
        sys.path.insert(0, d)

import mavutil

# Implementing MainFrameBase
class mixer_document( ):
    def __init__( self, aircraftName ):

        # determine if application is a script file or frozen exe
        if hasattr(sys, 'frozen'):
            self.application_path = os.path.dirname(sys.executable)
        elif __file__:
            self.application_path = os.path.dirname(__file__)
            
        self.aircraftName = aircraftName
            
        self.data_path = os.path.join(self.application_path, "..", "..", "data", "mixer");
        
        self.settings_path = os.path.join(self.data_path, "Settings.xml")
        self.Settings = FESettings.parse(self.settings_path)
        
        self.function_blocks_path = os.path.join(self.data_path, "FunctionBlocks.xml")
        self.FBlocksMain = FBlocksAPI.parse(self.function_blocks_path)
        self.FBlocks = self.FBlocksMain.get_functionBlock()
        
        self.exportPath = ''
        
        self.m_openProject( )
        
        self.auto_update = False

        self.callbacks = []
        
    # Status handling
    
    def m_set_autoUpdate(self, set_on):
        self.auto_update = set_on
    
    def m_register_callback(self, callback):
        self.callbacks.append(callback)
        
    def m_call_callbacks(self, callback_type, hint = None):
        for callback in self.callbacks:
            callback(callback_type, hint)
            
    def m_sync_in_progress(self):
        self.m_call_callbacks(callback_type.SYNC_IN_PROGRESS)

    def m_sync_complete(self):
        self.m_call_callbacks(callback_type.SYNC_COMPLETE)

    def m_sync_fail(self):
        self.m_call_callbacks(callback_type.SYNC_FAIL)

    def m_not_syncronised(self):
        self.m_call_callbacks(callback_type.NOT_SYNCHRONISED)

    def m_connected(self):
        self.m_call_callbacks(callback_type.ONLINE)

    def m_disconnected(self):
        self.m_call_callbacks(callback_type.OFFLINE)
        
    def m_NM_write_ok(self):
        self.m_call_callbacks(callback_type.NV_MEM_WRITE_OK)

    def m_NM_write_fail(self):
        self.m_call_callbacks(callback_type.NV_MEM_WRITE_FAIL)

    # Document handling
    
    def m_openProject( self, filePath = None):
        if(filePath != None):
            self.Settings.ProjectPath = filePath
        
        if(os.path.isfile(self.Settings.ProjectPath)):
            self.Project = FEProject.parse(self.Settings.ProjectPath)
        else:
            default_path = os.path.join(self.data_path, "DefaultProject.fep")
            self.Project = FEProject.parse(default_path)

        if(os.path.isfile(self.Project.FunctionSettingsPath)):
            self.m_openSettingsFile(self.Project.FunctionSettingsPath)
        else:
            self.Project.FunctionSettingsPath = os.path.join(self.data_path, "DefaultSettings.feset")
            self.m_openSettingsFile(self.Project.FunctionSettingsPath)        


    def m_openSettingsFile( self, filepath ):
        self.MAVFSettings = MAVFSettingsAPI.parse(filepath)

        self.selectedFunctionIndex = 0
        self.selectedRegisterIndex = 0

        self.Project.FunctionSettingsPath = filepath
        self.registers = self.MAVFSettings.registers.register

        
        if(self.MAVFSettings.get_inputRegs() == None):
            try:
                inputRegs = MAVFSettingsAPI.inputsSub()
                self.MAVFSettings.set_inputRegs(inputRegs)

                inputReg = MAVFSettingsAPI.inputSub("PWIN_ROLL")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("PWIN_PITCH")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("PWIN_YAW")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("PWIN_THROTTLE")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("PWIN_FLAP")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("PWIN_CAMBER")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("PWIN_BRAKE")
                self.MAVFSettings.inputRegs.input.append(inputReg)

                inputReg = MAVFSettingsAPI.inputSub("APCON_ROLL")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_PITCH")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_YAW")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_THROTTLE")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_FLAP")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_CAMBER")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_BRAKE")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APCON_WAGGLE")
                self.MAVFSettings.inputRegs.input.append(inputReg)

                inputReg = MAVFSettingsAPI.inputSub("APMODE_FULL")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("RADIO_MANUAL_MODE")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("APMODE_RADIO_ON")
                self.MAVFSettings.inputRegs.input.append(inputReg)
                inputReg = MAVFSettingsAPI.inputSub("GAIN_MAN_MIX")
                self.MAVFSettings.inputRegs.input.append(inputReg)

            except:
                print("summat wrong")
                
        
        if(self.MAVFSettings.get_outputRegs() == None):
            try:
                outputRegs = MAVFSettingsAPI.outputsSub()
                self.MAVFSettings.set_outputRegs(outputRegs)

                outputReg = MAVFSettingsAPI.outputSub("AILERON_L")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("ELEVATOR")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("THROTTLE")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("RUDDER")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("AILERON_R")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("FLAPMID_L")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("FLAPMID_R")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("FLAP_L")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("FLAP_R")
                self.MAVFSettings.outputRegs.output.append(outputReg)
                outputReg = MAVFSettingsAPI.outputSub("SPOILER")
                self.MAVFSettings.outputRegs.output.append(outputReg)
            except:
                print("summat else wrong")
            
            self.m_call_callbacks(callback_type.UPDATE_ALL)

    def m_saveSettingsFile( self, filepath ):
        if filepath == "":
            filepath = self.Project.FunctionSettingsPath;

        FILE = open(filepath, "w")

        self.MAVFSettings.export( FILE , 0 )


    def m_findRegisterIndexWithName ( self, regName ):
        index = 0
        for item in self.registers:
            if regName == item.identifier:
                return index
            index = index + 1
        return -1

    def m_findTypeIndexWithName ( self, typeName ):
        print('Searching for function type ', typeName)
        index = 0
        for FBlock in self.FBlocks:
            if FBlock.header.name == typeName:
                return index
            index = index + 1
        return -1

            
    def m_selFunctionAtIndex ( self, index ):
        self.selectedFunctionIndex = index
        self.m_refreshParametersGrid()

    def m_clearSelectedFunctionParamList ( self ):
        del self.MAVFSettings.functions.function[self.selectedFunctionIndex].setting[:]
        print("clear parameters from function")

    def m_changeSelectedFunctionType ( self, functionTypeIndex ):
        self.m_clearSelectedFunctionParamList()
        prntstr = 'Change selected function type, function{:d}, type index{:d}'.format(self.selectedFunctionIndex, functionTypeIndex)
        print(prntstr)
        sourceFBlock = self.FBlocks[functionTypeIndex]
        self.MAVFSettings.functions.function[self.selectedFunctionIndex].header.functionType = sourceFBlock.header.name
        print("Setting function to name ", self.MAVFSettings.functions.function[self.selectedFunctionIndex].header.functionType)
        for item in self.FBlocks[functionTypeIndex].setting:
            newParameter = MAVFSettingsAPI.functionBlockDataSub(item.name, item.default)
            self.MAVFSettings.functions.function[self.selectedFunctionIndex].setting.append(newParameter)
            print("insert new parameter into function")
        
        self.m_call_callbacks(callback_type.FUNCTION_CHANGED, self.selectedFunctionIndex)


    def m_menuGetUniqueRegisterName ( self ):
        found = False
        index = 1
        
        while found == False:
            searchStr = 'NULL_{:d}'.format(index)
            match = False
            for register in self.registers:
                if register.identifier == searchStr:
                    match = True
            if match == False:
                found = True
            index = index + 1
        return searchStr
            
                
    
    # Handlers for MainFrameBase events.
    def m_selectRegister( self, registerIndex):
        self.selectedRegisterIndex = registerIndex
        
    def m_addRegister ( self ):
        regstring = self.m_menuGetUniqueRegisterName()          #'NULL_{:d}'.format(len(self.registers) + 1)
        newreg = MAVFSettingsAPI.registerSub(regstring, "Does nothing")
        self.registers.append(newreg)
        self.m_call_callbacks(callback_type.REGISTERS_CHANGED)

    def m_insertRegister ( self ):
        regstring =  self.m_menuGetUniqueRegisterName()         #'NULL_{:d}'.format(len(self.registers) + 1)
        newreg = MAVFSettingsAPI.registerSub(regstring, "Does nothing")
        self.registers.insert( self.selectedRegisterIndex, newreg)
        self.m_call_callbacks(callback_type.REGISTERS_CHANGED)

    def m_menuDeleteRegister ( self ):
        if(len(self.registers) > 1):
            self.registers.pop(self.selectedRegisterIndex)
            self.selectedRegisterIndex = 0
            self.m_call_callbacks(callback_type.REGISTERS_CHANGED)

    def m_addFunction ( self ):
        newFHeader = MAVFSettingsAPI.functionBlockHeaderSub("NULL", "NULL", "CLEAR", "Do nothing")
        newFSettings = []
        newfunc = MAVFSettingsAPI.functionSub(newFHeader, newFSettings)
        self.MAVFSettings.functions.function.append(newfunc)
        self.m_call_callbacks(callback_type.FUNCTIONS_CHANGED, 0)

    def m_menuInsertFunction ( self ):
        newFHeader = MAVFSettingsAPI.functionBlockHeaderSub("NULL", "NULL", "CLEAR", "Do nothing")
        newFSettings = []
        newfunc = MAVFSettingsAPI.functionSub(newFHeader, newFSettings)
        self.MAVFSettings.functions.function.insert(self.selectedFunctionIndex, newfunc)
        self.m_call_callbacks(callback_type.FUNCTIONS_CHANGED, self.selectedFunctionIndex)

    def m_menuDeleteFunction ( self ):
        if(len(self.MAVFSettings.functions.function) > 1):
            self.MAVFSettings.functions.function.pop(self.selectedFunctionIndex)
            self.selectedFunctionIndex = 0
            self.m_call_callbacks(callback_type.FUNCTIONS_CHANGED, self.selectedFunctionIndex)


    def   m_change_regName ( self, col, newRegName ):
        if newRegName.find(" ") != wx.NOT_FOUND:
            print("No spaces allowed in register names")
            return False
        if len(newRegName) > 15:
            print("Name too long, reduce to less than 15 characters, no spaces")
            return False
        if len(newRegName) == 0:
            print("Must be at least one character long")
            return False
        self.MAVFSettings.registers.register[ col ].identifier = newRegName
        self.m_call_callbacks(callback_type.FUNCTIONS_CHANGED, self.selectedFunctionIndex)
        
        return True


    def   m_setAction ( self, actionStr ):
        self.MAVFSettings.functions.function[self.selectedFunctionIndex].header.action = actionStr
        self.m_call_callbacks(callback_type.FUNCTION_MODIFIED, self.selectedFunctionIndex)

    def   m_paramSelect ( self, index ):
        self.m_paramsSelectIndex = index

    def   m_paramChange ( self, functionIndex, paramIndex,  paramEditStr, paramTypeName):
            
        if paramTypeName == 'Register':
            if self.m_findRegisterIndexWithName( paramEditStr ) == -1:
                print("ERROR: Could not find register with name " + paramEditStr )
                print("Reset editor value")
                return False
        if paramTypeName == 'Percent':
            try:
                percent = float(paramEditStr)
                if percent > 150:
                    print("Percent over 150, Reset editor value")
                    return False
                if percent < -150:
                    print("Percent under -150, Reset editor value")
                    return False
            except ValueError:
                print("Invalid value, Reset editor value")
                return False
        if paramTypeName == 'int16':
            try:
                int16 = int(paramEditStr)
                if int16 > 32767:
                    print("int16 over range, Reset editor value")
                    return False
                if int16 < -32767:
                    print("int16 under range, Reset editor value")
                    return False
            except ValueError:
                print("Invalid value, Reset editor value")
                return False
        if paramTypeName == 'int14':
            try:
                int14 = int(paramEditStr)
                if int14 > 8192:
                    print("int14 over range, Reset editor value")
                    return False
                if int14 < -8192:
                    print("int14 under range, Reset editor value")
                    return False
            except ValueError:
                print("Invalid value, Reset editor value")
                return False
        #if paramTypeName == 'Fractional':aa
 
        self.MAVFSettings.functions.function[functionIndex].setting[paramIndex].value = paramEditStr
        self.m_call_callbacks(callback_type.UPDATE_FUNCTION, functionIndex)
        #        print('Changing function ', self.selectedFunctionIndex, ' parameter ', event.GetRow(), ' to ', editStr)


    def m_update ( self ):
        self.m_call_callbacks(callback_type.UPDATE_ALL)
        
    def m_openSettings( self, filePath ):
        self.m_openSettingsFile(filePath)
            
    def m_saveSettings( self, event ):
        self.m_saveSettingsFile("")

    def m_saveSettingsAs( self, filePath ):
        self.m_saveSettingsFile(filePath)

            
    def m_saveProject( self ):
        FILE = open(self.Settings.ProjectPath, "w")
        if(not FILE.closed):            
            try:
                self.Project.export( FILE , 0 )
            except:
                print("could not export project file")
        

    def m_saveProjectAs( self, filePath ):
        FILE = open(filePath, "w")
        try:
            self.Project.export( FILE , 0 )
        except:
            print("could not export project")
        else:
            self.Settings.ProjectPath = filePath


    def m_mnExportCHeaders( self, filePath):
        self.exportPath = filePath
        Files = CFileGen.CFiles()
        Files.writeFiles(self.exportPath, "FlexiFunciton", self.MAVFSettings, self.FBlocks)
            
    def m_mnCommitToNV(self, event ):
        try:
            self.MAVProcesses
        except:
            return False
   
        self.MAVProcesses.commit_buffer_to_nvmem();
        return True
            
    def m_close( self ):
        FILE = open(self.Settings.ProjectPath, "w")
        if(not FILE.closed):            
            try:
                self.Project.export( FILE , 0 )
            except:
                print("could not export project file")
                
        FILE = open(self.settings_path, "w")
        if(not FILE.closed):            
            try:
                self.Settings.export( FILE , 0 )
            except:
                print("could not export settings file")
                
    

