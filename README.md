# linux_fd628

This repository contains the source code for the FD628 and compatible LED controllers.
Controller datasheet: http://pdf1.alldatasheet.com/datasheet-pdf/view/232882/PTC/PT6964.html
The FD628 controller is often used in Android TV boxes with a 7 segment LED display at the front.

Although the FD628 controller has support for a multitude of different display configurations,
the current implementation only supports common cathode and common anode displays with 7 segments.
The driver can be extended to support other display types and configurations if the need arises.

The original source code was provided by the the manufacturer of Tanix TV boxes, and can be found
here: https://github.com/tanixbox/tx3mini_linux_fd628
