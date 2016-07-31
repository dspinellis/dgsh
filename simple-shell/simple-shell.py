from subprocess import Popen, PIPE, STDOUT
import sys
from socket import socketpair, AF_UNIX, SOCK_DGRAM
from os import pipe, fork, close, execlp, dup, dup2, \
        open as osopen, O_WRONLY, O_CREAT, environ
from collections import OrderedDict
import re

def debug(s):
  if DEBUG:
    sys.stderr.write(s)


class Process:
  processes = {}

  def __init__(self, command):
    self.command = command
    self.inputConnectors = []
    self.outputConnectors = []
    self.fileDescriptorsInUse = []

  def selectInputFileDescriptor(self):
    fd = -1
    if 0 not in self.fileDescriptorsInUse:
      fd = 0
    elif 0 in self.fileDescriptorsInUse and \
         3 not in self.fileDescriptorsInUse:
      fd = 3
    else:
      fd = self.fileDescriptorsInUse[-1] + 1
    self.fileDescriptorsInUse.append(fd)
    debug("input file descriptor return: %d\n" % fd)
    return fd

  def selectOutputFileDescriptor(self):
    fd = -1
    if 1 not in self.fileDescriptorsInUse:
      fd = 1
    elif 1 in self.fileDescriptorsInUse and \
         3 not in self.fileDescriptorsInUse:
      fd = 3
    else:
      fd = self.fileDescriptorsInUse[-1] + 1
    self.fileDescriptorsInUse.append(fd)
    debug("output file descriptor return: %d\n" % fd)
    return fd

def setupProcess(index, channel, connector):
  debug("index: %d, toolDict[index]: %s, channel: %s\n" % \
            (index, toolDict[index], channel))
  try:
    if channel == 'output':
      # index - 1: offset because index = 1, 2,...
      Process.processes[index].outputConnectors.append(connector)
    elif channel == 'input':
      Process.processes[index].inputConnectors.append(connector)
  except KeyError:
    Process.processes[index] = Process(toolDict[index])
    setupProcess(index, channel, connector)

def parse(command):
  if '\'' and '"' not in command:
    return command.split()
  else:
    splits = []
    splitS = True
    splitD = True
    for pos, letter in enumerate(command):
      if letter == "'":
        if splitS:
          splitS = False
        else:
          splitS = True
      elif letter == '"':
        if splitD:
          splitD = False
        else:
          splitD = True
      elif letter is ' ' and splitS and splitD:
        splits.append(pos)
    print splits
    args = []
    prev = 0
    for pos in splits:
      args.append(command[prev:pos].replace("'", ""))
      prev = pos+1
    args.append(command[prev:].replace("'", ""))
    return args

# Debug configuration
DEBUG = False
try:
  if sys.argv[2] == "DEBUG" or sys.argv[3] == "DEBUG":
    DEBUG = True
except IndexError:
  pass

# Get output file name
try:
  outFile = sys.argv[2]
except IndexError:
  outFile = ""

# Read specification of processes and their interconnections
try:
  sgshGraph = sys.argv[1]
except IndexError:
  print "Input error: please specify an input file with tool and pipe specifications."
  exit(1)
with open(sgshGraph, 'r') as f:
  lines = f.readlines()

toolDefsEnd = 0
for index, line in enumerate(lines):
  if line == "%\n":
    toolDefsEnd = index
    break
if toolDefsEnd == 0:
  print "Failed to find tool definition end line (\n%\n)"
  exit(1)
debug("toolDefsEnd: %s\n" % toolDefsEnd)

toolDict = {}
for line in lines[:toolDefsEnd]:
  if line == '\n':
    continue
  match = re.match(r'^(\d+) (.+)\n$', line)
  if not match:
    print "Did not match command index and description: %s\n" % line
    exit(1)
  toolIndex = match.group(1)
  toolCommand = match.group(2)
  toolDict[int(toolIndex)] = toolCommand
  debug("index: %s, command: %s\n" % \
          (toolIndex, toolCommand))

connectorDict = OrderedDict()
for line in lines[toolDefsEnd+1:]:
  match = re.match(r'^(\w+) (\d+) (\d+)\n$', line)
  if not match:
    print "Did not match connector description and endpoints: %s\n" % line
    exit(1)
  connector = match.group(1)
  fromIndex = match.group(2)
  toIndex = match.group(3)
  connectorDict[int('%s%s' % (fromIndex, toIndex))] = connector
  debug("connector: %s, from index: %s, to_index: %s\n" % \
          (connector, fromIndex, toIndex))

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
  node_index_out = processPair / 10
  node_index_inp = processPair % 10
  debug("out: %d, inp: %d, connector[0]: %d, connector[1]: %d\n" % (node_index_out, node_index_inp, connectorPair[0].fileno(), connectorPair[1].fileno()))
  setupProcess(node_index_out, 'output', connectorPair)
  setupProcess(node_index_inp, 'input', connectorPair)

# Open output file
if outFile:
  outfile_fd = osopen(outFile, O_WRONLY | O_CREAT)

# Activate interconnections and execute processes
for index, process in Process.processes.iteritems():
  debug('process %s, input channels: %d, output channels: %d\n' \
         % (process.command, len(process.inputConnectors), \
            len(process.outputConnectors)))
  pid = fork()
  if pid:
    debug("%s: inputConnectors: %d\n" % (process.command, len(process.inputConnectors)))
    if process.inputConnectors:
        environ["SGSH_IN"] = "1"
    else:
        environ["SGSH_IN"] = "0"
    for ic in process.inputConnectors:
      fd = process.selectInputFileDescriptor()
      try:
        close(fd)
      except OSError:
        print "FAIL: close input fd %d for process %s. Discard and move on" \
                % (fd, process.command)
      fd = dup(ic[1].fileno())
      debug("%s: dup %d, gives %d\n" % (process.command, ic[1].fileno(), fd))
      ic[1].close()
      ic[0].close()
    debug("%s: outputConnectors: %d\n" % (process.command, len(process.outputConnectors)))
    if process.outputConnectors:
        environ["SGSH_OUT"] = "1"
    else:
        environ["SGSH_OUT"] = "0"
    for oc in process.outputConnectors:
      fd = process.selectOutputFileDescriptor()
      debug("%s: fd selected: %d, fd brought: %d\n" % (process.command, process.fileDescriptorsInUse[-1], oc[0].fileno()))
      try:
        close(fd)
      except OSError:
        print "FAIL: close output fd %d for process %s. Discard and move on" \
                % (fd, process.command)
      fd = dup(oc[0].fileno())
      debug("%s: dup %d, gives %d\n" % (process.command, oc[0].fileno(), fd))
      oc[0].close()
      oc[1].close()
    if not process.outputConnectors and outFile:
      close(1)
      dup(outfile_fd)
      close(outfile_fd)
    args = parse(process.command)
    execlp(args[0], args[0], *args[1:])
