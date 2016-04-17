from subprocess import Popen, PIPE, STDOUT
import sys
from socket import socketpair
from os import pipe, fork, getpid, close, execlp, dup, waitpid, WNOHANG


class Process:
  processes = []

  def __init__(self, command):
    self.command = command
    self.inputConnectors = []
    self.outputConnectors = []

def setupProcess(index, channel, connector):
  try:
    if channel == 'output':
      Process.processes[index].outputConnectors.append(connector)
    elif channel == 'input':
      Process.processes[index].inputConnectors.append(connector)
  except IndexError:
    if len(Process.processes) == index - 1:
      Process.processes.append(Process(toolDict[index]))
      setupProcess(index - 1, channel, connector)
    else:
      print 'Error in specification of connector: %s %s %s' \
             % (connector, processPair[0], processPair[1])
      exit(1)

def selectInputFileDescriptor(index):
  if index == 0:
    return index
  else:
    return index + 2

def selectOutputFileDescriptor(index):
  return index + 1

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
    connectorPair[0], connectorPair[1] = socketpair()
  elif connector == 'pipe':
    connectorPair[0], connectorPair[1] = pipe()
  else:
    print 'Do not understand connector %s' % connector
    exit(1)
  out = int(processPair[0])
  inp = int(processPair[1])
  setupProcess(out, 'output', connectorPair)
  setupProcess(inp, 'input', connectorPair)

# Activate interconnections and execute processes
pids = []
for i, process in enumerate(Process.processes):
  print 'process %s, input channels: %d, output channels: %d' \
         % (process.command, len(process.inputConnectors), \
            len(process.outputConnectors))
  pids.append(fork())
  if pids[-1]:
    for index, ic in enumerate(process.inputConnectors):
      close(selectInputFileDescriptor(index))
      dup(ic[1].fileno())
      ic[1].close()
      ic[0].close()
    for index, oc in enumerate(process.outputConnectors):
      close(selectOutputFileDescriptor(index))
      dup(oc[0].fileno())
      oc[0].close()
      oc[1].close()
    args = process.command.split()
    execlp(args[0], args[0], *args[1:])

for pid in pids:
  waitpid(pid, 0)

