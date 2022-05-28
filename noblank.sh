#!/bin/bash

NOBLANK="
@xset s noblank
@xset s off
@xset -dpms
"

  # Pretty ANSI text colors
  OFF="\033[0m"
  BOLD="\033[1m"
  DIM="\033[2m"
  RED="\033[1;31m"
  GREEN="\033[1;32m"
  YELLOW="\033[1;33m"
  BLUE="\033[1;34m"

  clear
  printf "\n\n\t $YELLOW           "; date ; printf "$OFF"
  printf "\n $GREEN"
  printf "\t +------------------------------------------------+\n"
  printf "\t |                  no_blank.sh                   |\n"
  printf "\t |                rev Oct 1, 2013                 |\n"
  printf "\t |                                                |\n"
  printf "\t |  by: Jan Zumwalt - net-wrench.com              |\n"
  printf "\t |                                                |\n"
  printf "\t |  This script permanently disables the xsession |\n"
  printf "\t |  screen saver for RASPBIAN PI OS. It allows    |\n"
  printf "\t |  the user to abort before any change is made.  |\n"
  printf "\t +------------------------------------------------+$OFF\n\n"

  printf "\t This program will disable your xsession\n"
  printf "\t screen saver permanently, is this what \n"
  printf "\t you really want to do? $YELLOW<y/n>$OFF"; read -n 1 KEYIN

  if [[ $KEYIN == "N"  ||  $KEYIN == "n" ]]; then
    printf "\n\n\t OK, I quit and did not do anything.\n\n"
    exit 0
  fi

  printf "\n\n\t I intend to modify your current \n"
  printf "\t /etc/xdg/lxsession/LXDE/autostart \n"
  printf "\t I will add the$GREEN GREEN$OFF lines to the file...\n\n"

  printf "$DIM\n"
  pr -t -o 9 /etc/xdg/lxsession/LXDE/autostart
  printf "$OFF$GREEN$NOBLANK" | pr -t -o 9
  printf "$OFF\n\n" 

  printf "\t Is this really what you want to do? $YELLOW<y/n>$OFF"; read -n 1 KEYIN

  if [[ $KEYIN == "N"  ||  $KEYIN == "n" ]]; then
    printf "\n\n\t OK, I quit and did not do anything.\n\n"
    exit 0
  fi

  cat /etc/xdg/lxsession/LXDE/autostart > /etc/xdg/lxsession/LXDE/autostart.`date +%m-%d-%Y_%I:%M:%S`
  printf "\n\t Created backup copy of the autostart file...\n"
  printf "$NOBLANK" >> /etc/xdg/lxsession/LXDE/autostart

  printf "\t Your new file looks like this\n"
  printf "$DIM\n"
  pr -t -o 9 /etc/xdg/lxsession/LXDE/autostart
  printf "$OFF\n\n"

  printf "\t$RED Modified file. Screensaver is now disabled.$OFF\n"

  # required so xterm will not close
  printf "\n\t press any key to exit: "; read -n 1 KEYIN
  printf "\t$GREEN Program ended normaly $OFF\n\n"
