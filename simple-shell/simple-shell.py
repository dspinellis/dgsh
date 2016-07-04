from subprocess import Popen, PIPE, STDOUT
import sys
from socket import socketpair, AF_UNIX, SOCK_DGRAM
from os import pipe, fork, close, execlp, dup, open as osopen, O_WRONLY, O_CREAT


class Process:
  processes = []

  def __init__(self, command):
    self.command = command
    self.inputConnectors = []
    self.outputConnectors = []
    self.fileDescriptorsInUse = []

  def selectInputFileDescriptor(self):
    fd = -1
    if not self.fileDescriptorsInUse:
      fd = 0
    elif 0 in self.fileDescriptorsInUse and \
         3 not in self.fileDescriptorsInUse:
      fd = 3
    else:
      fd = self.fileDescriptorsInUse[-1] + 1
    self.fileDescriptorsInUse.append(fd)
    #print "input file descriptor return: %d" % fd
    return fd

  def selectOutputFileDescriptor(self):
    fd = -1
    if not self.fileDescriptorsInUse:
      fd = 1
    elif 1 in self.fileDescriptorsInUse and \
         3 not in self.fileDescriptorsInUse:
      fd = 3
    else:
      fd = self.fileDescriptorsInUse[-1] + 1
    self.fileDescriptorsInUse.append(fd)
    #print "output file descriptor return: %d" % fd
    return fd

def setupProcess(index, channel, connector):
  #print "index: %d, toolDict[index]: %s, channel: %s\n" % \
  #          (index, toolDict[index], channel)
  try:
    if channel == 'output':
      # index - 1: offset because index = 1, 2,...
      Process.processes[index - 1].outputConnectors.append(connector)
    elif channel == 'input':
      Process.processes[index - 1].inputConnectors.append(connector)
  except IndexError:
    if len(Process.processes) == index - 1:
      Process.processes.append(Process(toolDict[index]))
      setupProcess(index, channel, connector)
    else:
      print 'Error in specification of connector: %s %s %s' \
             % (connector, processPair[0], processPair[1])
      exit(1)

# Get output file name
try:
  outFile = sys.argv[2]
except IndexError:
  print "Input error: please specify a file name to store the output."
  exit(0)

# Read specification of processes and their interconnections
try:
  sgshGraph = sys.argv[1]
except IndexError:
  print "Input error: please specify an input file with tool and pipe specifications."
  exit(0)
with open(sgshGraph, 'r') as f:
  lines = f.read()
toolsConnectors = lines.split('%')
toolDict = {}
tools = toolsConnectors[0].lstrip().rstrip().split('\n')
for tool in tools:
  toolRecord = tool.split(' ', 1)
  toolDict[int(toolRecord[0])] = toolRecord[1]
connectorDict = {}
connectors = toolsConnectors[1].lstrip().rstrip().split('\n')
for connector in connectors:
  connectorRecord = connector.split()
  connectorDict['%s%s' % (connectorRecord[1], connectorRecord[2])] = \
                                                            connectorRecord[0]

# Setup objects that represent objects along with their interconnections
for processPair, connector in connectorDict.iteritems():
  # convention: connector[0]: output, connector[1]: input
  connectorPair = [None, None]
  if connector == 'socketpipe':
    connectorPair[0], connectorPair[1] = socketpair(AF_UNIX, SOCK_DGRAM)
  elif connector == 'pipe':
    connectorPair[0], connectorPair[1] = pipe()
  else:
    print 'Do not understand connector %s' % connector
    exit(1)
  out = int(processPair[0])
  inp = int(processPair[1])
  #print "out: %d, inp: %d" % (out, inp)
  setupProcess(out, 'output', connectorPair)
  setupProcess(inp, 'input', connectorPair)

# Open output file
outfile_fd = osopen(outFile, O_WRONLY | O_CREAT)

# Activate interconnections and execute processes
for process in Process.processes:
  #print 'process %s, input channels: %d, output channels: %d, file descriptors in use: %d' \
  #       % (process.command, len(process.inputConnectors), \
  #          len(process.outputConnectors), len(process.fileDescriptorsInUse))
  pid = fork()
  if pid:
    #print "inputConnectors: %d" % len(process.inputConnectors)
    for ic in process.inputConnectors:
      close(process.selectInputFileDescriptor())
      fd = dup(ic[1].fileno())
      #print "%s: close %d, dup %d, gives %d" % (process.command, process.fileDescriptorsInUse[-1], ic[1].fileno(), fd)
      ic[1].close()
      ic[0].close()
    #print "outputConnectors: %d" % len(process.outputConnectors)
    for oc in process.outputConnectors:
      close(process.selectOutputFileDescriptor())
      #print "%d <--> %d" % (process.fileDescriptorsInUse[-1], oc[0].fileno())
      dup(oc[0].fileno())
      oc[0].close()
      oc[1].close()
    if not process.outputConnectors:
      close(1)
      dup(outfile_fd)
      close(outfile_fd)
    args = process.command.split()
    execlp(args[0], args[0], *args[1:])
