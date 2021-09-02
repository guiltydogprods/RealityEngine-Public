
package com.guiltydog;

import android.os.Build;
import android.os.Bundle;
import android.app.NativeActivity;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;
import android.widget.Button;
import android.content.pm.PackageManager;

public class REQuestActivity extends android.app.NativeActivity
{
    protected void onCreate(Bundle savedInstanceState)
    {
		super.onCreate(savedInstanceState);
		org.fmod.FMOD.init(this);
    }

    protected void onDestroy()
    {
		org.fmod.FMOD.close();
        super.onDestroy();
    }

	static
	{
		System.loadLibrary("vrapi");
		System.loadLibrary("ovrplatformloader");
		System.loadLibrary("ovravatarloader");
		System.loadLibrary("fmodL");
		System.loadLibrary("BlocksRuntime");
	}
}
