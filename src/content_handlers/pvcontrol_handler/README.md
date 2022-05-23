# PVControl Handler

The PVControl Handler is a relatively simple content handler that serves as the glue between the Azure Device Update agent and a [Pantavisor Linux](https://www.pantavisor.io/) device.

## Overview

The ADU agent with the PVControl Handler runs inside a [container](https://gitlab.com/pantacor/pv-platforms/adu-agent) in a [Pantavisor-enabled](https://docs.pantahub.com/index.html) device. It can process any updates formed by a bsp (Linux Kernel, drivers, firmware and the [Pantavisor](https://docs.pantahub.com/index.html) binary) plus a number of containers. These containers mostly provide the application level of the device, but can also just be utilities that enhance the functionality of Pantavisor, such as the adu-agent container.

Internally, it works in a similar way as SWUpdate Handler, with just a manifest json pointing to a tarball that contains the artifacts for each update. This updates can be produced with the help of the [pvr2adu](#pvr2adu) script, which takes a [pvr checkout](https://docs.pantahub.com/make-a-new-revision.html) as input and outputs the manifest and tarball that will be consumed by Azure Device Update UI.

After that, the PVControl Handler takes the tarball and installs and applies it with the help of the [pvcontrol](https://gitlab.com/pantacor/pv-platforms/pvr-sdk/-/blob/master/files/usr/bin/pvcontrol) script. The pvcontrol script is just a very simple HTTP client that communicates using a UNIX socket with [Pantavisor](https://docs.pantahub.com/pantavisor-commands.html), which is the core of the device and is in charge of orchestrating the running containers and managing the updates.

## pvr2adu

# pvr2adu

The script is located in ```tools/pvr2adu```. It can be used either to install the ADU client in a [Pantavisor-enabled](https://docs.pantahub.com/index.html) device, get an image with Pantavisor and an already installed ADU client to flash a new device or convert a [pvr checkout](https://docs.pantahub.com/make-a-new-revision.html) into ADU format.

```
Usage: pvr2adu [options] <download|add-agent|export> [arguments]
Convert pvr checkout and images into ADU format
options:
       -h this help
       -v verbose
```

To install it, just copy the file in tools/pvr2adu and place it in your $PATH:

```
mkdir -p ~/bin
cp tools/pvr2adu ~/bin/pvr2adu
chmod +x ~/bin/pvr2adu
export PATH=$PATH:~/bin
```
