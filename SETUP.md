# CURIO ISLES — setup

This machine is already set up (it built EMBERHOME): Unreal Engine 5.8 with the Android target platform,
Visual Studio 2022, Android Studio + SDK/NDK, JDK 21 for Gradle. The full recipe, every Android fix and
the Google Play release steps are in `C:\SACHIN\Hollowlight\PLAYBOOK.md` and `C:\SACHIN\Hollowlight\SETUP.md`;
the scripts in `Tools/Build` are the same ones, renamed for this game.

## Everyday

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1          # physics + levels, seconds
powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1    # build + Unreal automation tests
powershell -ExecutionPolicy Bypass -File Tools\Validation\capture.ps1      # screenshots of every screen
```

Play it from the editor build:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\SACHIN\CurioIsles\CurioIsles.uproject" -game -windowed -ResX=1600 -ResY=900
```

## Test on an Android phone

1. On the phone: Settings → About → tap *Build number* 7 times; then Developer options → USB debugging on.
2. Build a test APK (first build takes a while):
   ```
   powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Config Development
   ```
3. Plug the phone in and run the generated `Packaged\Android\Install_CurioIsles_universal.bat`
   (or copy the `.apk` to the phone and tap it).

## Release (milestone M4)

1. Once, create the upload key yourself (you type the password; nothing goes into git):
   `powershell -ExecutionPolicy Bypass -File Tools\Build\create_upload_key.ps1` — back up
   `Build\Android\curioisles-upload.keystore` and `Config\Android\AndroidEngine.ini`.
2. `powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release` → signed `.aab`.
3. Play Console: new personal accounts need a closed test with 12+ testers for 14 days before production.
   The app id `com.brainrotinteractive.curioisles` can never change after the first upload.
4. In-app purchase (island unlock) needs `bSupportsInAppPurchasing=True` and the Google Play billing
   plugin — planned for M4, off until then.
