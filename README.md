## Linux driver for SINOWEALTH Genesis Xenon 750 mouse

This driver was made with SINOWEALTH Genesis Xenon 750 mouse (258a:1007) in mind and was not tested for
the other Genesis products.

Tested on Arch Linux with kernel 6.10.3-lqx1-1-lqx.

## The driver allows you to

- set USB polling rate,
- change the functionality of each button,
- set macro.

### For each DPI mode

- change DPI,
- change logo and scroll wheel color,
- disable specific DPI modes.

## How to use

```
Usage: ./driver [ARG...]

Order of the arguments matter and should be placed with order like below.
<config_file>               path to the config file
(optional) <bus_number>     bus number of the mouse
(optional) <port_number>    port number of the mouse
```
It is necessary to run the driver as root, otherwise libusb will have insufficient
permissions to open USB devices.

To customize mouse you need to provide the driver with the path of a config file.
In the mouse_config.cfg file you can find a config file template with default mouse settings
and instructions for each section.

If you have more than one Xenon 750 mouse you can pass bus_number and port_number
arguments to the driver to select the right mouse.

Bus number and port number can be found using lsusb command. First we need to find device number.
```
lsusb
```
```
...
Bus 001 Device 006: ID 258a:1007 SINOWEALTH Game Mouse
```

The device number is 6. Now use lsusb with option -t.
```
lsusb -t
```
```
Bus 001.Port 001: Dev 001, Class=root_hub, Driver=xhci_hcd/12p, 480M
    |__ ...
    |__ Port 011: Dev 006, If 0, Class=Human Interface Device, Driver=usbhid, 12M
``` 

The device with number 6 is on bus 1 and port 11, that is our mouse bus and port number. 

## How to build

`make`

## Dependencies

For the driver to work 2 dependencies are required: [libconfig](https://github.com/hyperrealm/libconfig) and [libusb](https://github.com/libusb/libusb).

They can be installed using a package manager such as pacman.
```
sudo pacman -S libconfig libusb
```

You can also build them from source.

