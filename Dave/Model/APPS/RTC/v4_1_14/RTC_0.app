<?xml version="1.0" encoding="ASCII"?>
<ResourceModel:App xmi:version="2.0" xmlns:xmi="http://www.omg.org/XMI" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:ResourceModel="http://www.infineon.com/Davex/Resource.ecore" name="RTC" URI="http://resources/4.1.14/app/RTC/0" description="Provides timing and alarm functions using RTC in the calendar time format." version="4.1.14" minDaveVersion="4.0.0" instanceLabel="RTC_0" appLabel="">
  <upwardMapList xsi:type="ResourceModel:RequiredApp" href="../../FATFS/v4_0_26/FATFS_0.app#//@requiredApps.0"/>
  <properties singleton="true" provideInit="true" sharable="true"/>
  <virtualSignals name="event_timer" URI="http://resources/4.1.14/app/RTC/0/vs_gcu_intrtctick_int" hwSignal="int" hwResource="//@hwResources.0"/>
  <virtualSignals name="event_alarm" URI="http://resources/4.1.14/app/RTC/0/vs_gcu_intrtcalarm_int" hwSignal="int" hwResource="//@hwResources.1"/>
  <requiredApps URI="http://resources/4.1.14/app/RTC/0/appres_clk_xmc4" requiredAppName="CLOCK_XMC4" requiringMode="SHARABLE">
    <downwardMapList xsi:type="ResourceModel:App" href="../../CLOCK_XMC4/v4_0_22/CLOCK_XMC4_0.app#/"/>
  </requiredApps>
  <requiredApps URI="http://resources/4.1.14/app/RTC/0/appres_global_scu" requiredAppName="GLOBAL_SCU_XMC4" required="false" requiringMode="SHARABLE"/>
  <requiredApps URI="http://resources/4.1.14/app/RTC/0/appres_cpu" requiredAppName="CPU_CTRL_XMC4" required="false" requiringMode="SHARABLE"/>
  <hwResources name="GCU RTC TICK" URI="http://resources/4.1.14/app/RTC/0/hwres_gcu_rtctick" resourceGroupUri="peripheral/scu/0/gcu/interrupt/rtctick" mResGrpUri="peripheral/scu/0/gcu/interrupt/rtctick">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/SCU/SCU_0.dd#//@provided.16"/>
  </hwResources>
  <hwResources name="GCU RTC ALARM" URI="http://resources/4.1.14/app/RTC/0/hwres_gcu_rtcalarm" resourceGroupUri="peripheral/scu/0/gcu/interrupt/rtcalarm" mResGrpUri="peripheral/scu/0/gcu/interrupt/rtcalarm">
    <downwardMapList xsi:type="ResourceModel:ResourceGroup" href="../../../HW_RESOURCES/SCU/SCU_0.dd#//@provided.19"/>
  </hwResources>
  <connections URI="http://resources/4.1.14/app/RTC/0/http://resources/4.1.14/app/RTC/0/vs_gcu_intrtctick_int/http://resources/4.1.14/app/RTC/0/vs_cpuctrl_app_node" systemDefined="true" sourceSignal="event_timer" required="false" srcVirtualSignal="//@virtualSignals.0" containingProxySignal="true"/>
  <connections URI="http://resources/4.1.14/app/RTC/0/http://resources/4.1.14/app/RTC/0/vs_gcu_intrtcalarm_int/http://resources/4.1.14/app/RTC/0/vs_cpuctrl_app_node" systemDefined="true" sourceSignal="event_alarm" required="false" srcVirtualSignal="//@virtualSignals.1" containingProxySignal="true"/>
  <connections URI="http://resources/4.1.14/app/RTC/0/http://resources/4.1.14/app/RTC/0/vs_gcu_intrtctick_int/http://resources/4.1.14/app/RTC/0/vs_global_scu_xmc4_node" systemDefined="true" sourceSignal="event_timer" required="false" srcVirtualSignal="//@virtualSignals.0" containingProxySignal="true"/>
  <connections URI="http://resources/4.1.14/app/RTC/0/http://resources/4.1.14/app/RTC/0/vs_gcu_intrtcalarm_int/http://resources/4.1.14/app/RTC/0/vs_global_scu_xmc4_node" systemDefined="true" sourceSignal="event_alarm" required="false" srcVirtualSignal="//@virtualSignals.1" containingProxySignal="true"/>
</ResourceModel:App>
