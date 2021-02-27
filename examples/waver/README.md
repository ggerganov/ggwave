# Waver

Waver allows you to send and receive text messages from nearby devices through sound waves.

This application can be used to communicate with multiple nearby devices at once. Both audible and ultrasound communication protocols are available. The app does not connect to the internet and all information is transmitted only through sound. In order to receive incoming messages you only need to allow access to your device's microphone so that it can record nearby sounds.

The main purpose of this app is to showcase the capabilities of the [ggwave](https://github.com/ggerganov/ggwave/) library. It is a convenient way to quickly test the transmission performance on virtually any device with speakers and a microphone.

### Install 

<a href="https://apps.apple.com/us/app/waver-data-over-sound/id1543607865?itsct=apps_box&amp;itscg=30200&ign-itsct=apps_box#?platform=iphone" style="display: inline-block; overflow: hidden; border-radius: 13px; width: 250px; height: 83px;"><img height="60px" src="https://tools.applemediaservices.com/api/badges/download-on-the-app-store/white/en-US?size=250x83&amp;releaseDate=1607558400&h=8e5fafc57929918f684abc83ff8311ef" alt="Download on the App Store"></a>
<a href='https://play.google.com/store/apps/details?id=com.ggerganov.Waver&pcampaignid=pcampaignidMKT-Other-global-all-co-prtnr-py-PartBadge-Mar2515-1'><img alt='Get it on Google Play' src='https://i.imgur.com/BKDCbKv.png' height="60px"/></a>
<a href="https://snapcraft.io/waver">
<img alt="Get it from the Snap Store" src="https://snapcraft.io/static/images/badges/en/snap-store-black.svg" height="60px"/>
</a>

#### Linux

```bash
sudo snap install waver
sudo snap connect waver:audio-record :audio-record
```

#### Mac OS

```bash
brew install ggerganov/ggerganov/waver
```

#### Run directly in the browser

https://waver.ggerganov.com

## How to use

Click on the gif to watch a ~2 min Youtube video:

<a href="https://youtu.be/Zcgf77T71QM"><img width="100%" src="../../media/waver-preview1.gif"></img></a>

- Before starting - make sure the speaker of your device is enabled and disconnect/unplug any headphones. The app uses your device's speaker to emit sounds when sending a text message
- To send a message - tap on "Messages", enter some text at the bottom of the screen and click "Send"
- Any nearby device that is also running this application can capture the emitted sound and display the received message
- In the settings menu, you can adjust the volume and the transmission protocol that will be used when sending messages. Make sure to adjust the volume level high enough, so the sounds can be picked up by other devices
- Tap on "Spectrum" to see a real-time frequency spectrum of the currently captured audio by your device's microphone


## File sharing in a local network

As of v1.3.0 Waver supports file sharing. It works like this:

- Add files that you would like to transmit by sharing them with Waver
- In the "Files" menu, click on "Broadcast". This plays an audio message that contains a file broadcast offer
- Nearby devices in the same local network can receive this offer and initiate a TCP/IP connection to your device
- The files are transmitted over TCP/IP. The sound message is used only to initiate the network connections between the devices
- Waver allows sharing multiple files to multiple devices at once

## Known issues

- The browser version does not support on-screen keyboard on mobile devices, so it is not possible to input messages. Use the mobile app instead
- In some cases utlrasound transmission is not supported (see [#5](https://github.com/ggerganov/ggwave/issues/5))
