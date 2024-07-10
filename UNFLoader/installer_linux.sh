#!/bin/bash

# Setup useful variables
Green='\033[0;32m'
ColorOff='\033[0m'
USERNAME=`whoami`

# Introduce the script
echo -e -n ${Green}
echo "This script will set up udev rules for supported UNFLoader flashcarts, as well as install UNFLoader itself to /usr/local/bin/."
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

# Check if the user wants UDEV rules
echo
echo -n -e ${Green}
echo "I would like to install udev rules."
echo "This will allow you to run UNFLoader without needing to call sudo."
while true; do
    UDEVUPDATED=false
    read -p "Would you like to setup udev rules? (y/n) " yn
    case $yn in
        [Yy]* )
            # 64Drive
            TARGET_OLD="30-64drive.rules"
            TARGET="31-64drive.rules"
            echo
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET_OLD}" ]; then
                sudo rm "/etc/udev/rules.d/${TARGET_OLD}"
                UDEVUPDATED=true
            fi
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for 64Drive. Skipping."
            else
                read -p "Would you like to setup udev rules for the 64Drive? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"64drive USB device\", OWNER=\"${USERNAME}\"" >> ${TARGET}
                        sudo mv ${TARGET} "/etc/udev/rules.d"
                        UDEVUPDATED=true
                        ;;
                    [Nn]* ) ;;
                    * ) echo "Please answer yes or no.";;
                esac
            fi

            # EverDrive
            TARGET_OLD="30-everdrive.rules"
            TARGET="31-everdrive.rules"
            echo
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET_OLD}" ]; then
                sudo rm "/etc/udev/rules.d/${TARGET_OLD}"
                UDEVUPDATED=true
            fi
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for EverDrive. Skipping."
            else
                echo "Would you like to setup udev rules for the EverDrive?"
                echo "Please be aware that due to the fact that the ED's device information was not modified by the manufacturer (KRIKzz), it shows up as a generic FTDI device."
                echo "This means that any devices which happen to share the same generic information will also not require sudo to access anymore."
                read -p "Continue? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"FT245R USB FIFO\", OWNER=\"${USERNAME}\"" >> ${TARGET}
                        sudo mv ${TARGET} "/etc/udev/rules.d"
                        UDEVUPDATED=true
                        ;;
                    [Nn]* ) ;;
                    * ) echo "Please answer yes or no.";;
                esac
            fi

            # SC64
            TARGET_OLD1="30-sc64.rules"
            TARGET_OLD2="30-sc64_phenom.rules"
            TARGET="31-sc64.rules"
            echo
            if [ -f "/etc/udev/rules.d/${TARGET_OLD1}" ]; then
                sudo rm "/etc/udev/rules.d/${TARGET_OLD1}"
                UDEVUPDATED=true
            fi
            if [ -f "/etc/udev/rules.d/${TARGET_OLD2}" ]; then
                sudo rm "/etc/udev/rules.d/${TARGET_OLD2}"
                UDEVUPDATED=true
            fi
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for SC64. Skipping."
            else
                echo "Would you like to setup udev rules for the SC64?"
                read -p "Continue? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"SC64\", ATTRS{idVendor}==\"0403\", ATTRS{idProduct}==\"6014\", OWNER=\"${USERNAME}\"" >> ${TARGET}
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
                echo "If your flashcart is currently plugged in, unplug it and plug it back so that the new rules are applied to the USB device."
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
        [Nn]* ) break;;
        * ) echo "Please answer yes or no.";;
    esac
done
echo -n -e ${Green}

# Done!
echo
echo -e ${Green}"UNFLoader installation finished. Have a nice day!"${ColorOff}