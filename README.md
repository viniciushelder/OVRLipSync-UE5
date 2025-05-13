# OVRLipSync-UE5
Oculus Lip Sync Plugin precompiled for Unreal 5.
  
![LipSync Demo](https://scontent.fcpq1-1.fna.fbcdn.net/v/t39.2365-6/64637900_622042114964562_89558726775668736_n.gif?_nc_cat=105&ccb=1-7&_nc_sid=ad8a9d&_nc_ohc=hKPnmX3MZakAX-Q0OOA&_nc_ht=scontent.fcpq1-1.fna&oh=00_AfCGDS8a7l1Ffr80ubuidQhW3tJ7afrObERULATH_XNuTQ&oe=64906D40 "LipSync Demo")

As you might know, the plugin available from the link in the offical docs don't work in Unreal Engine 5. I fixed it with the help of a lot of internet strangers and compiled the plugin. Tested on Quest2/3 and Quest Pro.
## How to use:

1. Copy the <mark>OVRLipSync</mark> folder in this repository to the Plugins folder in your Unreal Engine 5 project. From the **Edit** menu, select **Plugins** and then **Audio**. You should see the **Oculus Lipsync** plugin as one of the options. Select Enabled to enable the plugin for your project.

2. Add the following to your **DefaultEngine.ini**:
```ini
[Voice]
bEnabled=true
``` 

3. Check the official [Docs](https://developer.oculus.com/documentation/unreal/audio-ovrlipsync-unreal/)


## License
This plugin is the property of [Meta](https://about.meta.com/) and is provided under the Oculus SDK License, which allows for personal and commercial use of the plugin. By using this plugin, you agree to the terms of the Oculus SDK License.
