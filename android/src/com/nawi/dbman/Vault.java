package com.nawi.dbman;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.graphics.Insets;
import android.os.Build;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Android Keystore AES-256-GCM vault + small platform helpers called from C++ via JNI. */
public final class Vault {
    private static final String ALIAS = "nawidbman_vault";

    private Vault() {}

    private static SecretKey key() throws Exception {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);
        if (!ks.containsAlias(ALIAS)) {
            KeyGenerator g = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
            g.init(new KeyGenParameterSpec.Builder(ALIAS,
                    KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256).build());
            g.generateKey();
        }
        return (SecretKey) ks.getKey(ALIAS, null);
    }

    /** Returns base64(iv || ciphertext), or null on failure. */
    public static String encrypt(String plain) {
        try {
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.ENCRYPT_MODE, key());
            byte[] iv = c.getIV();
            byte[] ct = c.doFinal(plain.getBytes(StandardCharsets.UTF_8));
            byte[] out = new byte[iv.length + ct.length];
            System.arraycopy(iv, 0, out, 0, iv.length);
            System.arraycopy(ct, 0, out, iv.length, ct.length);
            return Base64.encodeToString(out, Base64.NO_WRAP);
        } catch (Throwable t) {
            return null;
        }
    }

    /** Returns plaintext, or null on failure (tampered data / lost key). */
    public static String decrypt(String enc) {
        try {
            byte[] all = Base64.decode(enc, Base64.NO_WRAP);
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, all, 0, 12));
            return new String(c.doFinal(all, 12, all.length - 12), StandardCharsets.UTF_8);
        } catch (Throwable t) {
            return null;
        }
    }

    /** ARGB of Android 12 dynamic accent, or 0 when unavailable. */
    public static int accent(Context ctx, boolean dark) {
        try {
            return ctx.getColor(dark ? android.R.color.system_accent1_200 : android.R.color.system_accent1_600);
        } catch (Throwable t) {
            return 0;
        }
    }

    /** "top,bottom,left,right,imeBottom" in px, or "" if not yet attached. */
    public static String insets(Context ctx) {
        try {
            View v = ((Activity) ctx).getWindow().getDecorView();
            WindowInsets wi = v.getRootWindowInsets();
            if (wi == null) return "";
            Insets s = wi.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            Insets i = wi.getInsets(WindowInsets.Type.ime());
            return s.top + "," + s.bottom + "," + s.left + "," + s.right + "," + i.bottom;
        } catch (Throwable t) {
            return "";
        }
    }

    /** light=true: dark status/navigation icons (for light backgrounds). */
    public static void setLightBars(Context ctx, final boolean light) {
        try {
            final Activity a = (Activity) ctx;
            a.runOnUiThread(new Runnable() {
                public void run() {
                    WindowInsetsController c = a.getWindow().getInsetsController();
                    if (c == null) return;
                    int m = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
                            | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
                    c.setSystemBarsAppearance(light ? m : 0, m);
                }
            });
        } catch (Throwable t) { /* cosmetic only */ }
    }

    /** Writes text to cache/exports/<name> and opens the system share sheet. */
    public static boolean shareText(Context ctx, String name, String text, String mime) {
        try {
            final Activity a = (Activity) ctx;
            File dir = new File(a.getCacheDir(), "exports");
            if (!dir.isDirectory() && !dir.mkdirs()) return false;
            File f = new File(dir, new File(name).getName());   // strip any path part
            try (FileOutputStream o = new FileOutputStream(f)) {
                o.write(text.getBytes(StandardCharsets.UTF_8));
            }
            final Uri uri = androidx.core.content.FileProvider.getUriForFile(a, a.getPackageName() + ".nawishare", f);
            final Intent send = new Intent(Intent.ACTION_SEND);
            send.setType(mime);
            send.putExtra(Intent.EXTRA_STREAM, uri);
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            a.runOnUiThread(new Runnable() {
                public void run() { a.startActivity(Intent.createChooser(send, null)); }
            });
            return true;
        } catch (Throwable t) {
            return false;
        }
    }
}
