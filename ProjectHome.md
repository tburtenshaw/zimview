**ZimView** is a program designed to view, edit and create firmware updates for the [Zinwell](Zinwell.md) and [Olevia](Olevia.md) digital TV [set-top boxes](SetTopBox.md).

These boxes are used in [New Zealand](NewZealand.md), [Taiwan](Taiwan.md) and [Brazil](Brazil.md), and often rebranded. The firmware can be updated over the air, or from a USB drive plugged into the back of the box.

The firmware is stored as a .zim file. Each .zim file contains a number of [blocks](ZimBlocks.md) which contain data to be copied to the storage on the set-top box.

Some of these .zim files can be found on the internet. Particularly the [Olevia](Olevia.md) firmware who seem to regularly update software.

## Using ZimView ##
ZimView can open these .zim files, display the constituent blocks, and extract their contents. It can also insert new blocks, and rearrange the order of existing blocks. It does all this while maintaining various checksums to ensure the file is not rejected by the set-top box.

![http://zimview.googlecode.com/svn/wiki/zimview_example.png](http://zimview.googlecode.com/svn/wiki/zimview_example.png)