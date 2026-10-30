package dev.axmol.renderdemand;

import android.os.Bundle;
import android.view.WindowManager;
import dev.axmol.lib.AxmolActivity;

public class AppActivity extends AxmolActivity
{
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
}
