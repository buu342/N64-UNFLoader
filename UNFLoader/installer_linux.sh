#!/bin/bash

# Setup useful variables
Green='\033[0;32m'
ColorOff='\033[0m'
D2XX_32="https://ftdichip.com/wp-content/uploads/2022/07/libftd2xx-x86_32-1.4.27.tgz"
D2XX_64="https://ftdichip.com/wp-content/uploads/2022/07/libftd2xx-x86_64-1.4.27.tgz"
OS_BIT=`getconf LONG_BIT`
EXECLOC=`pwd`
USERNAME=`whoami`

# Introduce the script
echo -e -n ${Green}
echo "This script will install the D2XX driver for UNFLoader, as well as set up udev rules."
echo "These operations will require sudo."
while true; do
    read -p "Would you like to continue? (y/n) " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

# Request Sudo
sudo -S echo

# Check if D2XX should be installed
if [ -f "/usr/local/lib/libftd2xx.so" ]; then
    echo "D2XX driver is already installed, skipping installation."
else
    while true; do
        read -p "Would you like to install the D2XX driver? (y/n) " yn
        case $yn in
            [Yy]* )
                echo "Downloading D2XX ($OS_BIT bits)"
                TARGET=$D2XX_32
                echo -e -n ${ColorOff}
                if [ "$OS_BIT" = "64" ]; then
                    TARGET=$D2XX_64
                fi

                # Create a working directory
                mkdir libftd2xx
                cd libftd2xx

                # Download and extract it
                wget $TARGET
                tar -xvf libftd2xx*

                # Move the library files to the right place, then create symbolic links
                sudo cp release/build/lib* /usr/local/lib
                cd /usr/local/lib
                sudo ln -s libftd2xx.so.* libftd2xx.so
                sudo chmod 0755 libftd2xx.so

                # Cleanup
                cd "$EXECLOC"
                rm -rf libftd2xx
                echo -e ${Green}"D2XX driver successfully installed!"
                break;;
            [Nn]* ) break;;
            * ) echo "Please answer yes or no.";;
        esac
    done
fi

# Check if the user wants UDEV rules
echo
echo -n -e ${Green}
echo "Now, I would like to install udev rules."
echo "This will allow you to run UNFLoader without needing to call sudo, and without needing to explicitly unbind the ftdi_sio driver."
echo "The udev rules will be setup for this user only (${USERNAME})."
while true; do
    UDEVUPDATED=false
    read -p "Would you like to setup udev rules? (y/n) " yn
    case $yn in
        [Yy]* )
            # 64Drive
            TARGET="30-64drive.rules"
            echo
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for 64Drive. Skipping."
            else
                read -p "Would you like to setup udev rules for the 64Drive? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"64drive USB device\", OWNER=\"${USERNAME}\"" >> ${TARGET}
                        echo "ATTRS{product}==\"64drive USB device\", RUN{program}+=\"/bin/bash -c 'echo \$kernel > /sys/bus/usb/drivers/ftdi_sio/unbind'\"" >> ${TARGET}
                        sudo mv ${TARGET} "/etc/udev/rules.d"
                        UDEVUPDATED=true
                        ;;
                    [Nn]* ) ;;
                    * ) echo "Please answer yes or no.";;
                esac
            fi

            # EverDrive
            TARGET="30-everdrive.rules"
            echo
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for EverDrive. Skipping."
            else
                echo "Would you like to setup udev rules for the EverDrive?"
                echo "Please be aware that due to the fact that the ED's device information was not modified by the manufacturer, it shows up as a generic FTDI device."
                echo "This means that any devices which happen to share the same generic information will be switched to the FTDI driver."
                read -p "Continue? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"FT245R USB FIFO\", OWNER=\"${USERNAME}\"" >> ${TARGET}
                        echo "ATTRS{product}==\"FT245R USB FIFO\", RUN{program}+=\"/bin/bash -c 'echo \$kernel > /sys/bus/usb/drivers/ftdi_sio/unbind'\"" >> ${TARGET}
                        sudo mv ${TARGET} "/etc/udev/rules.d"
                        UDEVUPDATED=true
                        ;;
                    [Nn]* ) ;;
                    * ) echo "Please answer yes or no.";;
                esac
            fi

            # SC64
            TARGET="30-sc64.rules"
            echo
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for SC64. Skipping."
            else
                echo "Would you like to setup udev rules for the SC64?"
                echo "Please be aware that this udev rule will prevent the official SC64 tools from working, since they do not use the FTDI driver."
                read -p "Continue? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"SC64\", ATTRS{manufacturer}==\"Polprzewodnikowy\", OWNER=\"${USERNAME}\"" >> ${TARGET}
                        echo "ATTRS{product}==\"SC64\", ATTRS{manufacturer}==\"Polprzewodnikowy\", RUN{program}+=\"/bin/bash -c 'echo \$kernel > /sys/bus/usb/drivers/ftdi_sio/unbind'\"" >> ${TARGET}
                        sudo mv ${TARGET} "/etc/udev/rules.d"
                        UDEVUPDATED=true
                        ;;
                    [Nn]* ) ;;
                    * ) echo "Please answer yes or no.";;
                esac
            fi

            # Reload udev rules
            if [ ${UDEVUPDATED} = true ]; then
                echo 
                echo -e -n ${ColorOff}
                echo "udev rules updated. Reloading..."
                sudo udevadm control --reload-rules && udevadm trigger > /dev/null 2>&1
                sudo udevadm control --reload
                echo "Done!"
            fi
            break;;
        [Nn]* ) break;;
        * ) echo "Please answer yes or no.";;
    esac
done

# Move UNFLoader to /bin
echo
echo -n -e ${Green}
echo "Would you like to move UNFLoader to /usr/local/bin/?"
echo "This will let you run UNFLoader from any folder."
while true; do
    read -p "Continue? (y/n) " yn
    case $yn in
        [Yy]* ) 
            echo -e -n ${ColorOff}
            sudo cp -f "UNFLoader" "/usr/local/bin/UNFLoader"
            break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done
echo -n -e ${Green}

# Done!
echo
echo -e ${Green}"UNFLoader installation finished. Have a nice day!"${ColorOff}