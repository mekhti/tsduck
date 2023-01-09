#!/usr/bin/env python
#----------------------------------------------------------------------------
#
# TSDuck sample Python application : manipulate PSI/SI tables,
# convert between binary sections, XML and JSON tables.
#
# All command line arguments are interpreted as input file (.xml or .bin)
# and are loaded as initial content.
#
#----------------------------------------------------------------------------

import tsduck, sys

# A Python class which handles TSDuck log messages.
# Here, there is no multi-threaded TSProcessor, so we use a synchronous Report.
# If the application does not need to handle the messages, tsduck.StdErrReport can
# be used for simplicity, instead of a user-defined class.
class Logger(tsduck.AbstractSyncReport):

    # Constructor.
    def __init__(self, severity = tsduck.Report.Info):
        super().__init__(severity)

    # This method is invoked each time a message is logged by TSDuck.
    def log(self, severity, message):
        print(f"==> {tsduck.Report.header(severity)}{message}")

# Main program: create a SectionFile.
rep = Logger(tsduck.Report.Verbose)
duck = tsduck.DuckContext(rep)
file = tsduck.SectionFile(duck)

# If command line arguments are provided, load the corresponding files.
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} [bin_or_xml_file ...]")
    for i in range(1, len(sys.argv)):
        if sys.argv[i].endswith(".xml"):
            rep.info(f"loading XML file {sys.argv[i]}")
            file.loadXML(sys.argv[i])
        elif sys.argv[i].endswith(".bin"):
            rep.info(f"loading binary file {sys.argv[i]}")
            file.loadBinary(sys.argv[i])
        else:
            rep.error(f"unknown file type {sys.argv[i]}, ignored")

rep.info("After initial load: %d bytes, %d sections, %d tables" % (file.binarySize(), file.sectionsCount(), file.tablesCount()))

# Loading inline XML table.
file.loadXML("""<?xml version="1.0" encoding="UTF-8"?>
<tsduck>
  <PAT transport_stream_id="10">
    <service service_id="1" program_map_PID="100"/>
    <service service_id="2" program_map_PID="200"/>
  </PAT>
</tsduck>""")

rep.info("After inline XML load: %d bytes, %d sections, %d tables" % (file.binarySize(), file.sectionsCount(), file.tablesCount()))

# Convert to XML and JSON.
print("---- XML file content ----")
print(file.toXML())
print("---- JSON file content ----")
print(file.toJSON())

# Deallocate C++ resources (in reverse order from creation).
file.delete()
duck.delete()
rep.delete()
