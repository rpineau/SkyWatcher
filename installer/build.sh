#!/bin/bash

mkdir -p ROOT/tmp/Skywatcher_X2/
cp "../Skywatcher.ui" ROOT/tmp/Skywatcher_X2/
cp "../mountlist Skywatcher.txt" ROOT/tmp/Skywatcher_X2/
cp "../build/Release/libSkywatcher.dylib" ROOT/tmp/Skywatcher_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.Skywatcher_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 Skywatcher_X2.pkg
pkgutil --check-signature ./Skywatcher_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.Skywatcher_X2 --scripts Scritps --version 1.0 Skywatcher_X2.pkg
fi

rm -rf ROOT
