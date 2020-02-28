# Utility function for midas odb interface
import os

# Return the value from a given key (float)
def getValue( key ):
    cmd = "odbedit" + " -c" +  " \"" + "ls -v " + "'" + key + "'" + "\""
    # Get current setting
    stdout = os.popen(cmd);
    regCurr =  float(stdout.read());
    return regCurr;

# Return the value from a given key (non-int)
def getString( key ):
    cmd = "odbedit" + " -c" +  " \"" + "ls -v " + "'" + key + "'" + "\""
    # Get current setting
    stdout = os.popen(cmd);
    regCurr =  stdout.read().strip();
    return regCurr;

def getBool( key ):
    cmd = "odbedit" + " -c" +  " \"" + "ls -v " + "'" + key + "'" + "\""
    # Get current setting
    stdout = os.popen(cmd);
    regCurr =  stdout.read().strip();
    return (regCurr=="y");


# Set given value
def setValue( key, value ):
    cmd =  "odbedit" + " -c" +  " \"" + "set " + "'" + key + "' " + str(value)  + "\""
    os.system(cmd);

# Set given string value
def setStringValue( key, value ):
    cmd =  "odbedit" + " -c" +  " \"" + "set " + "'" + key + "' " + value  + "\""
#    print cmd
    os.system(cmd);

# Output text to midas
def msg( msgtype, txt ):
    cmd='odb -c "msg USER,%s %s"' % (msgtype,txt)
#    print cmd
    os.system(cmd)

# Stop given client
def stop(client):
    cmd='odb -c "shutdown \\"%s\\""' % (client)
    os.system(cmd)
