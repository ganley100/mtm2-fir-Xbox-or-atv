package com.mtm2.recomp;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

import java.io.File;

/**
 * SDL2's activity around the game. The native side (libmain.so) finds the
 * game in this app's folder on shared storage,
 * /sdcard/Android/data/com.mtm2.recomp/files/game, made here on first start
 * so it is there to copy the game into (docs/android.md).
 */
public class MtmActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File files = getExternalFilesDir(null);
        if (files != null) {
            new File(files, "game").mkdirs();
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        // SDL2_ttf is linked into libmain.so.
        return new String[] { "SDL2", "main" };
    }
}
