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
