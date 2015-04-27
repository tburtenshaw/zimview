# Introduction #
This article details how to get the [Olevia](Olevia.md) firmware on to your set-top box.

## Risks/disadvantages ##
  * **There is NO EASY WAY to return to the New Zealand firmware** - you would need equipment to read the NZ firmware off another machine, then use that equipment and a complicated computer programme to copy that to your box - not easy to do
  * Likely to void warranty (if you tell them)
  * [Electronic programme guide](ElectronicProgrammeGuide.md) will occasionally act differently
  * Possibility of ruining your set-top box
  * No access to future updates to New Zealand firmware
  * SD output stuck on [NTSC](NTSC.md) - this cannot be changed to [PAL](PAL.md) - this means that if you run your TV or other device off composite (yellow cable) or s-video then your display may not work
  * I have not properly checked on subtitles - if you need these desperately, perhaps delay installation

## Advantages ##
  * Simple (but limited) PVR functionality
  * High-definition component video output
  * Knowledge of exactly what software is running on your hardware
  * Olevia seems to have a much better upgrade policy than [Freeview](Freeview.md) - it has been updated more frequently, and aims to add functionality for the consumer

# Preparation #

You will need:
  * Access to the Olevia .zim files
  * A copy of [zimview](http://code.google.com/p/zimview) or at least a hex editor
  * A usb flash drive
  * Potentially a piece of paper and a pen to jot things down

## Zim files ##
We currently need three of the [Olevia](Olevia.md) .zim files. Note that this may change if new versions are released.
  * ZMT-620FTA Firmware v1.2.2 - Part 1 only (_OleviaPVR\_1.2.2\_part1.zim_)
  * ZMT-620FTA Firmware v1.8.2 - Part 2 only (_OleviaPVR\_1.8.2\_part 2.zim_)
  * ZMT-620FTA Firmware v.1.9.8 (_OLEVIA\_620\_1.9.8.zim_)

You can find them here: [ZMT-620FTA Firmware](http://www.olevia.com.hk/downloadpage.php?workplaceID=43&plid=&pid=119) - although they have recently added a password.

Or try this [torrent](http://www.h33t.com/details.php?id=dbe69fb4c311936b54a7babbcf2289845c349d2c).

You can also find them on [my downloads](http://code.google.com/p/zimview/downloads/list) page. They are licenced under the [GPL](GPL.md) and use a lot of GPL software (e.g. [Busybox](Busybox.md) and [Linux](Linux.md)) internally.

## Editing the first zim file ##
_The first step of editing the zim file is not always necessary. It depends which version of the NZ firmware you have, and whether you need to trick it. Try without this first step. If this doesn't work, then try the edit._

We need to change the [customer number](CustomerNumber.md) of our first file. This is to make the set-top box think we are installing New Zealand firmware. We only need to do this to the first file, since after that we will be installing onto [Olevia](Olevia.md) software.

The first file is _OleviaPVR\_1.2.2\_part1.zim_. Open this up with [zimview](http://code.google.com/p/zimview).

![http://zimview.googlecode.com/svn/wiki/edit_customernumber.png](http://zimview.googlecode.com/svn/wiki/edit_customernumber.png)

You should see a list of five different blocks (as above). Fortunately we can ignore these. The only thing we need to do is change the customer number.

  1. On the menu click on Block->Customer number...
  1. It will say 1280, change this to **4096**
  1. Click OK.
  1. Go to File->Save As
  1. Call the file something like _nz\_cust\_pvr122\_1.zim_ (that's what I called mine).

### Hex editing ###
Skip this section if the above worked. If you can't use my program for whatever reason, you can use a hex editor to change the customer number and update the checksum.

At the very start of the file change the first six bytes:
```
Change: a0 9b 88 85 00 05
To:     ce e8 88 90 00 10
```

Save this as _nz\_cust\_pvr122\_1.zim_.

## USB flash drive ##
Copy your newly edited file, and the two others to a thumb drive. For some reason my SD card reader didn't work, so I used an old 128MB thumb drive I found around the house.

The drive doesn't need to be empty, but you'll have to scroll through everything else on there. They also state that the drive contents can be erased, so you're best off starting fresh with an empty drive.

## Mental preparation ##
When you upgrade to the [Olevia](Olevia.md) firmware, you won't be able to automatically scan for all the New Zealand channels.

Fortunately, there are only three frequencies to tune to. Each frequency carries multiple "stations". To over-simplify, one channel basically carries the [TVNZ](TVNZ.md) stuff, another the [TV3/TV4](MediaWorks.md) stuff, the other channel contains the rest.

Make sure you know your frequencies, channels, bandwidth and priority. Try scanning the channels on your NZ firmware - you won't double up or lose any channels.

Each channel corresponds to a frequency - but these are different in Taiwan. Take a look at this page for a list of NZ channels and frequencies: http://www.freeviewshop.co.nz/digital-terrestrial-i-8.html

![http://zimview.googlecode.com/svn/wiki/manualscan_45.jpg](http://zimview.googlecode.com/svn/wiki/manualscan_45.jpg)

Bandwidth (at least in Auckland) is **8 MHz**, and Priority is **High**.

If you do an autoscan, keep a piece of paper nearby and write down the channels it finds.

![http://zimview.googlecode.com/svn/wiki/autoscan_auckland.jpg](http://zimview.googlecode.com/svn/wiki/autoscan_auckland.jpg)

In short - you're going to have to do this once you upgrade - so learn to do it now when it's safe.

# Process of upgrading #
Turn your set-top box on. Once it's running, stick in the USB drive with the three Olevia zim files. If you have a wide USB key, you may need to remove the HDMI cable and use component or composite output (or use a thinner drive). Don't proceed until you've sorted this out.

On the menu, go down to _System_, and then to _System Information_. You'll see information about your current firmware. Press the **red button** on your [remote control](RemoteControl.md) (_S/W Upgrade_).

![http://zimview.googlecode.com/svn/wiki/freeview_system_info_1.0.8.jpg](http://zimview.googlecode.com/svn/wiki/freeview_system_info_1.0.8.jpg)

## Steps ##
  1. Now a box should pop up saying _Select Slot_. Select **A: PORT1**.
  1. A new menu should appear: _Select Drive_. Unless you're doing something funny, you should see **A:Disk1**. Select this and press Enter.
  1. Now you will have a list of files. Choose your modified zim file (e.g. _nz\_cust\_pvr122\_1.zim_)
  1. There should now be a confirmation window. Make sure you've checked the correct file, then choose **Yes**. Take note of the colour of the selected item - you see how you've got to choose between _yes_ and _no_? The white text on dark blue is selected.

![http://zimview.googlecode.com/svn/wiki/confirm_yes.jpg](http://zimview.googlecode.com/svn/wiki/confirm_yes.jpg)

## Self-restart ##
You will now see an _Updating..._ window with a progress bar. The software will then write the data in the zim file to the firmware. Be patient. Don't touch the power button.

The machine will restart and will display a prompt for choosing the language. As you noted above, the selected item has a darker blue background. Choose **English**.

![http://zimview.googlecode.com/svn/wiki/language_english.jpg](http://zimview.googlecode.com/svn/wiki/language_english.jpg)

## Install the next two .zim files ##
You will notice the menu looks slightly different from the New Zealand firmware. The navigation is still basically the same though. You will also notice that none of channels are tuned in, and that an autotune will fail - this is expected.

We need to install the two remaining .zim files to get our Olevia firmware up to date. But before this feel free to have a play around tuning the channels - you can't really break anything doing this, and you can be confident they'll be reset once you upgrade the firmware.

Anyway, on the menu now go back to _System_->_System Information_. Push the **red button**. Sometimes the set-top box may not detect the USB drive without it being pulled out and put in again - try this if it's not detecting.

Follow the menus and select the second .zim file (OleviaPVR\_1.8.2\_part 2.zim). Install this. Select **English**.

![http://zimview.googlecode.com/svn/wiki/list_zims.jpg](http://zimview.googlecode.com/svn/wiki/list_zims.jpg)

Install the third file. After the restart, select **English**. You will now need to tune the channels and change the time zone.

# After the upgrade #
## Tuning ##
Hopefully you have an idea of what to do to get your channels tuned in.

Here's a summary:
  1. Press the _Menu button_ on the remote.
  1. Go to _Installation_.
  1. Go to _Manual Scan_.
  1. You need to enter in the information for the three channels (or muxes as they are known). Look them up for your area.

In Auckland:
  1. Enter Channel **29**, Frequency **538000**, Bandwidth **8 MHz**,  Priority **HIGH**. Move to **Start Scan** and press _Enter_ on the remote.
  1. Enter Channel **33**, Frequency **570000**, Bandwidth **8 MHz**,  Priority **HIGH**. **Start Scan**.
  1. Enter Channel **45**, Frequency **666000**, Bandwidth **8 MHz**,  Priority **HIGH**. **Start Scan**.

Each time you scan a UHF channel, you'll see the various stations carried in that frequency. When you are recording you can only watch programs of the same frequency.

![http://zimview.googlecode.com/svn/wiki/manualscan_33.jpg](http://zimview.googlecode.com/svn/wiki/manualscan_33.jpg)

## Time zone ##
You may need to set-up the time zone to New Zealand, otherwise your [EPG](ElectronicProgrammeGuide.md) will be incorrect.

  1. Press the _Menu button_ on the remote.
  1. Go to _Settings_.
  1. Go to _Time_.
  1. Change the time zone to **GMT+12:00**.

![http://zimview.googlecode.com/svn/wiki/timezone.jpg](http://zimview.googlecode.com/svn/wiki/timezone.jpg)

# Using the new firmware #

Now you've installed the Olevia firmware, have a look at UsingAndRecordingWithOleviaFirmware.