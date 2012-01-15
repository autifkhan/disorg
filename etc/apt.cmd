get the virtual box key
wget http://download.virtualbox.org/virtualbox/debian/oracle_vbox.asc
sudo apt-key add oracle_vbox.asc
sudo apt-add-repository 'deb http://download.virtualbox.org/virtualbox/debian natty contrib'

delete source line from /etc/apt/sources.list

wget http://winff.org/ubuntu/AAFE086A.gpg
sudo apt-key add AAFE086A.gpg
sudo apt-add-repository 'deb http://winff.org/ubuntu natty universe'

sudo cp /home/autif/data/etc/medibuntu.list /etc/apt/sources.list.d/
sudo apt-get --allow-unauthenticated install medibuntu-keyring

sudo apt-get update
sudo apt-get upgrade

sudo apt-get install -y ubuntu-restricted-extras eclipse virtualbox-4.0 xine-ui vlc mplayer dvdrip vim amule bluez-utils blueman xfce4-places-plugin xosview thttpd gnome-disk-utility gnome-system-monitor meld aumix libnotify-bin aptitude winff x264 libavcodec-extra-52 dvdrip eclipse openoffice.org gtkpod hal upslug2 smbfs twitux bison flex texinfo zsync git-all github-cli texi2html subversion chrpath libsdl1.2-dev jedit minicom screen mono-complete monodevelop ruby-full fbreader libqt4-dev sysstat

sudo /usr/share/doc/libdvdread4/install-css.sh

disable service thttpd

sudo apt-get build-dep -y pidgin

