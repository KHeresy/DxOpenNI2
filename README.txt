DxOpenNI2
=========

DxOpenNI.dll for MikuMiku Dance, OpenNI 2 and NiTE 2 version

========
See the detail in: 
  
http://kheresy.wordpress.com/2013/03/07/dxopenni-for-openni-2/
http://sdrv.ms/Zp3vhg

(In Traditional Chinese) 

If you have any problem, please reply in the blog post. (OK in English) 

DxOpenNI.dll is the module for MikuMikuDance ( http://www.geocities.jp/higuchuu4/index_e.htm ) to support Microsoft Kinect motion capture. The author of MMD developed it with OpenNI 1.x only, doesn't support new OpenNI 2.x. So I modify the 1.30 source code, replace the OpenNI 1.x code with OpenNI 2 and NiTE 2. To use this module, pleas install OpenNI 2 and NiTE 2 first. After install OpenNI 2 and NiTE 2, please copy all the files in the "Redist" directory, paste to the MMD directory.

Next, put the "DxOpenNI.dll" file to the "Data" directory in MMD. It should work now. 

Because I build this with VisualStudio 2012, user may need to install " Visual C++ Redistributable Packages". You can find the file in: http://www.microsoft.com/en-us/download/details.aspx?id=30679. Please download and install the file "vcredist_x86.exe". 

========
Note: 

1 Because MMD is a 32bit application, so you need to use 32bit version of OpenNI and NiTE, even if your OS is 64bit. 

2 The default "Redist" path of OpenNI and NiTE: 

2.1 32bit Windows¡G OpenNI: C:\Program Files\OpenNI2\Redist NiTE: C:\Program Files\PrimeSense\NiTE2\Redist 

2.2 64bit indows¡G OpenNI: C:\Program Files (x86)\OpenNI2\Redist NiTE: C:\Program Files (x86)\PrimeSense\NiTE2\Redist 


I also provide the source code, so you can modify it by yourself. To build this, you must have: OpenNI 2, NiTE 2, and Direct X SDK. 
