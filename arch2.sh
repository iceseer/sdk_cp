#!/bin/bash
read -p "Введите имя компьютера: " hostname
read -p "Введите имя пользователя: " username

echo 'Прописываем имя компьютера'
echo $hostname > /etc/hostname
ln -svf /usr/share/zoneinfo/Asia/Yekaterinburg /etc/localtime

echo '3.4 Добавляем русскую локаль системы'
echo "en_US.UTF-8 UTF-8" > /etc/locale.gen
echo "ru_RU.UTF-8 UTF-8" >> /etc/locale.gen 

echo 'Обновим текущую локаль системы'
locale-gen

echo 'Указываем язык системы'
echo 'LANG="en_US.UTF-8"' > /etc/locale.conf

echo 'Вписываем KEYMAP=en FONT=sun16'
echo 'KEYMAP=en' >> /etc/vconsole.conf
echo 'FONT=sun16' >> /etc/vconsole.conf

echo 'Создадим загрузочный RAM диск'
mkinitcpio -p linux

echo '3.5 Устанавливаем загрузчик'
pacman -Syu
pacman -S grub --noconfirm 
grub-install /dev/sda

echo 'Обновляем grub.cfg'
grub-mkconfig -o /boot/grub/grub.cfg

echo 'Ставим программу для Wi-fi'
pacman -S dialog wpa_supplicant --noconfirm 

echo 'Добавляем пользователя'
useradd -m -g users -G wheel -s /bin/bash $username

echo 'Создаем root пароль'
passwd

echo 'Устанавливаем пароль пользователя'
passwd $username

echo 'Устанавливаем SUDO'
echo '%wheel ALL=(ALL) ALL' >> /etc/sudoers

echo 'Раскомментируем репозиторий multilib Для работы 32-битных приложений в 64-битной системе.'
echo '[multilib]' >> /etc/pacman.conf
echo 'Include = /etc/pacman.d/mirrorlist' >> /etc/pacman.conf
pacman -Syu

echo 'Ставим иксы и драйвера. Ставим на ВМ?'
read -p "0 - VB, 1 - VMWARE, 2 - NO: " vm_setting
if [[ $vm_setting == 0 ]]; then
  gui_install="xorg-server xorg-drivers xorg-xinit linux-headers virtualbox-guest-utils"
elif [[ $vm_setting == 1 ]]; then
  gui_install="xorg-server xorg-drivers xorg-xinit xorg-utils xorg-server-utils mesa xorg-twm xterm xorg-xclock xf86-video-vmware"
elif [[ $vm_setting == 2 ]]; then
  gui_install="xorg-server xorg-drivers xorg-xinit"
fi
pacman -S $gui_install

echo "Какое DE ставим?"
read -p "1 - XFCE, 2 - KDE, 3 - Openbox, 4 - GNOME, 5 - Cinnamon: " vm_setting
if [[ $vm_setting == 1 ]]; then
  pacman -S xfce4 xfce4-goodies --noconfirm
elif [[ $vm_setting == 2 ]]; then
  pacman -Sy plasma-meta kdebase --noconfirm
elif [[ $vm_setting == 3 ]]; then  
  pacman -S  openbox xfce4-terminal
elif [[ $vm_setting == 4 ]]; then  
  pacman -S gnome gnome-extra --noconfirm
elif [[ $vm_setting == 5 ]]; then  
  pacman -S  cinnamon nemo-fileroller
fi

pacman -S pulseaudio pulseaudio-alsa pavucontrol

echo 'Cтавим DM'
pacman -S lxdm --noconfirm
systemctl enable lxdm

echo 'Ставим шрифты'
pacman -S ttf-liberation ttf-dejavu --noconfirm 

echo 'Ставим сеть'
pacman -S networkmanager network-manager-applet ppp --noconfirm

echo 'Подключаем автозагрузку менеджера входа и интернет'
systemctl enable NetworkManager

echo 'Установка завершена! Перезагрузите систему.'
echo 'Если хотите подключить AUR, установить мои конфиги XFCE, тогда после перезагрзки и входа в систему, установите wget (sudo pacman -S wget) и выполните команду:'
echo 'wget git.io/arch3.sh && sh arch3.sh'
exit
