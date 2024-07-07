#!/bin/bash

# Setup useful variables
Green='\033[0;32m'
ColorOff='\033[0m'

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
                        echo "ATTRS{product}==\"64drive USB device\", TAG+=\"uaccess\"" >> ${TARGET}
                        echo "ATTRS{product}==\"64drive USB device\", RUN{program}" >> ${TARGET}
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
                echo "Please be aware that due to the fact that the ED's device information was not modified by the manufacturer (KRIKzz), it shows up as a generic FTDI device."
                echo "This means that any devices which happen to share the same generic information will also not require sudo to access anymore."
                read -p "Continue? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"FT245R USB FIFO\", TAG+=\"uaccess\"" >> ${TARGET}
                        echo "ATTRS{product}==\"FT245R USB FIFO\", RUN{program}" >> ${TARGET}
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
                read -p "Continue? (y/n) " yn
                case $yn in
                    [Yy]* )
                        echo -e -n ${ColorOff}
                        echo "ATTRS{product}==\"SC64\", ATTRS{manufacturer}==\"Polprzewodnikowy\", TAG+=\"uaccess\"" >> ${TARGET}
                        echo "ATTRS{product}==\"SC64\", ATTRS{manufacturer}==\"Polprzewodnikowy\", RUN{program}" >> ${TARGET}
                        sudo mv ${TARGET} "/etc/udev/rules.d"
                        UDEVUPDATED=true
                        ;;
                    [Nn]* ) ;;
                    * ) echo "Please answer yes or no.";;
                esac
            fi
            TARGET="30-sc64_phenom.rules"
            echo
            echo -e -n ${Green}
            if [ -f "/etc/udev/rules.d/${TARGET}" ]; then
                echo "udev rules already setup for SC64 (Phenom Mods). Skipping."
            else
                echo -e -n ${ColorOff}
                echo "ATTRS{product}==\"SC64\", ATTRS{manufacturer}==\"Polprzewodnikowy/Mena\", TAG+=\"uaccess\"" >> ${TARGET}
                echo "ATTRS{product}==\"SC64\", ATTRS{manufacturer}==\"Polprzewodnikowy/Mena\", RUN{program}" >> ${TARGET}
                sudo mv ${TARGET} "/etc/udev/rules.d"
                UDEVUPDATED=true
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
        [Nn]* ) break;;
        * ) echo "Please answer yes or no.";;
    esac
done
echo -n -e ${Green}

# Done!
echo
echo -e ${Green}"UNFLoader installation finished. Have a nice day!"${ColorOff}