# Proof of oncept that esp32cam can work with camera, SD card and sim800Lv2 at the same time.

## Programming
The programming is done via USBtTTL module. The wiring is shown in the picture.

![alt text](https://github.com/DmitryLapshov/esp32cam_sim800Lv2_proof_of_concept/blob/main/esp32cam02.png)

## Approach
The main issue is that as soon as camera and SD card are activated it is impossible to use "free" pins for an additional HardwareSerial interface for comunication with GSM module.
On the other hand as soon as HardwareSerial is activated the camera fails to initialise.

As a workaround it is possible to use them sequentually, one after another. The proposed algorithm is this:
Start -> If a photo is needed -> Initialise Camera and SD card -> Take photo -> Store the photo on SD card -> Initialise HardwareSerial -> Send the photo via GPRS -> Either used GSM again or RESTART.
Start -> If GPRS is needed -> Initialise HardwareSerial -> Use GSM modem -> Either used GSM again or RESTART.

So the work with esp32cam is being done in "cycles". That way it becomes possible to control the esp32cam's behaviour by an external controller. It may be a computer connected via USB to TTL connecter or Arduino board sending "commands" to esp32cam via its additional SoftwareSerial interface.
