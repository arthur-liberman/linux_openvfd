# linux_openvfd

This repository contains the source code for the FD628 and similar LED display controllers,
[Controller datasheet](http://pdf1.alldatasheet.com/datasheet-pdf/view/232882/PTC/PT6964.html).

The FD628 controller is often used in Android TV boxes with a 7 segment LED display at the front.

Although the FD628 controller has support for a multitude of different display configurations,
the current implementation only supports common cathode and common anode displays with 7 segments.
The driver can also support FD650, which uses a variation of the I2C communication protocol.
In addition it supports HD44780 text based LCD displays, using an I2C backpack.
