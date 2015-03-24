#!/bin/bash

die() { echo "$@" 1>&2 ; exit 1; }

if [ -z "$1" ]; then
  die "Usage: $0 <version>"
fi
VERSION=$1

FILES="bin/dnscat2-linux-x32 bin/dnscat2-linux-x64 bin/dnscat2-win32.exe bin/dnscat2-win64.exe"

echo "Expected files:"
for i in $FILES; do
  echo "* $i"
done

echo "Cleaning up..."
make clean >/dev/null || die "Problem cleaning the sourcecode"

echo "Copying the client sourcecode to bin/..."
rm -rf bin/dnscat2-client-source
cp -r client/ bin/dnscat2-client-source || die "Failed to copy"
FILES="$FILES bin/dnscat2-client-source"

echo "Copying the server sourcecode to bin/..."
rm -rf bin/dnscat2-server
cp -r server/ bin/dnscat2-server || die "Failed to copy"
FILES="$FILES bin/dnscat2-server"

echo "Creating dist/ directory"
mkdir dist/ >/dev/null 2>&1

echo "Compressing files..."
ZIPS=""
for i in $FILES; do
  if [ -e "$i" ]; then
    OUTNAME="dist/$(basename $i)-$VERSION"

    tar -cjf $OUTNAME.tar.bz2 $i || die "Failed to create $i.tar.bz2"
    ZIPS="$ZIPS $OUTNAME.tar.bz2"

    tar -czf $OUTNAME.tgz     $i || die "Failed to create $i.tgz"
    ZIPS="$ZIPS $OUTNAME.tgz"

  else
    echo "Missing file warning: $i"
  fi
done

for i in $ZIPS; do
  gpg -q -b $i || die "Failed to sign $i"
done
