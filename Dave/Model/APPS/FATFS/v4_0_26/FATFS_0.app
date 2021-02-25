<?xml version="1.0" encoding="ASCII"?>
<ResourceModel:App xmi:version="2.0" xmlns:xmi="http://www.omg.org/XMI" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ResourceModel="http://www.infineon.com/Davex/Resource.ecore" name="FATFS" URI="http://resources/4.0.26/app/FATFS/0" description="This APP provides the FAT file system implementation &#xA;based on FatFs library (www.elm-chan.org) &#xA;which is a generic FAT file system module for small embedded systems." version="4.0.26" minDaveVersion="4.2.8" instanceLabel="FATFS_0" appLabel="">
  <properties singleton="true" provideInit="true" sharable="true"/>
  <requiredApps URI="http://resources/4.0.26/app/FATFS/0/appres_rtc" requiredAppName="RTC" requiringMode="SHARABLE">
    <downwardMapList xsi:type="ResourceModel:App" href="../../RTC/v4_1_14/RTC_0.app#/"/>
  </requiredApps>
  <requiredApps URI="http://resources/4.0.26/app/FATFS/0/appres_sdmmc_sd0" requiredAppName="SDMMC_BLOCK">
    <downwardMapList xsi:type="ResourceModel:App" href="../../SDMMC_BLOCK/v4_0_28/SDMMC_BLOCK_0.app#/"/>
  </requiredApps>
  <requiredApps URI="http://resources/4.0.26/app/FATFS/0/appres_sdmmc_sd1" requiredAppName="SDMMC_BLOCK" required="false"/>
  <requiredApps URI="http://resources/4.0.26/app/FATFS/0/appres_sdmmc_sd2" requiredAppName="SDMMC_BLOCK" required="false"/>
</ResourceModel:App>
