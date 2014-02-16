# delayman #
delayman is a filter driver that adds new functionality to the NT shutdown process. It listens for system buttons events, such as the closing of a laptop lid, and delays the immediate shutdown process with a configurable amount of time.

### Driver functionality ###
To achieve the shutdown manipulation functionality, the filter driver is installed as lower filter to a ACPI system button device (e.g. `ACPI Lid`). As a lower filter to this device, it is capable of intercepting the I/O Request Packets (IRPs) that are sent by the NT power manager. Whenever a system button event is detected, the completion of the appropriate IRP would normally "travel" back to the power manager which would initiate the system shutdown process. This driver makes sure that the IRP completion is delayed by a configurable amount of time.

During installation, the INF instructs the driver to install itself as a root-enumerated device and service (`Root\DelayMan`). A co-installer is used to enumerate the ACPI system button devices (specifically the `ACPI Lid` device) and to install the delayman driver as lower filter to these devices.

It's really pretty simple.. if you know where to look. There's not a lot of information available on these IRPs and events, so the driver serves as an educational example on how to intercept and manipulate IRPs (and should be able to be used by those that really want to delay their power buttons). Its use is at your own risk.

For more information on how the operating system handles IRPs and power buttons, have a look the following websites:

* [A Hole In My Head: How PS2 and HID keyboads report power button events](http://blogs.msdn.com/doronh/archive/2006/09/08/746961.aspx)
* [A Hole In My Head : How are power buttons reported in Windows?](http://blogs.msdn.com/doronh/archive/2006/09/08/746834.aspx)
* [Microsoft KB : How to disable the keyboard Sleep button with a filter driver](http://support.microsoft.com/default.aspx?scid=kb;en-us;302092&Product=win2000)

### KMDF drivers ###
The driver is built using the Microsoft [Kernel-Mode Driver Framework](http://www.microsoft.com/whdc/driver/wdf/KMDF.mspx) framework (version 1.7), which is part of the [Windows Driver Foundation](http://www.microsoft.com/whdc/driver/wdf/default.mspx). The KMDF makes the process of developing drivers for Microsoft Windows somewhat easier by providing an abstraction layer in between the operating system and the driver. Compared to traditional drivers, only a minimal amount of code is required in order to write a functional driver. Because the KMDF acts as an intermediate layer, drivers can also be kept compatible across various versions of Microsoft Windows.

### TODOs ###
The driver provides the basic functionality described above, but is not complete. Here's a list of things that still need to be done:
  * provide a tool (e.g. systray tool) to manipulate the driver's functionality
  * the driver needs a properly implemented WMI class for configuration
  * the shutdown delay is currently hardcoded and should be configurable through the tool (e.g. by using WMI)
  * a user needs to be able to stop the shutdown process; currently only implemented on NT 6.x by using the undocumented `KeAlertThread` function (oops!)
