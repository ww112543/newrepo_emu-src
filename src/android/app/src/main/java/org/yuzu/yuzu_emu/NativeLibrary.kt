// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.Html
import android.text.method.LinkMovementMethod
import android.view.Surface
import android.view.View
import android.widget.TextView
import androidx.annotation.Keep
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.lang.ref.WeakReference
import org.yuzu.yuzu_emu.YuzuApplication.Companion.appContext
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.utils.DocumentsTree.Companion.isNativePath
import org.yuzu.yuzu_emu.utils.FileUtil.exists
import org.yuzu.yuzu_emu.utils.FileUtil.getFileSize
import org.yuzu.yuzu_emu.utils.FileUtil.isDirectory
import org.yuzu.yuzu_emu.utils.FileUtil.openContentUri
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.SerializableHelper.serializable

/**
 * Class which contains methods that interact
 * with the native side of the Yuzu code.
 */
object NativeLibrary {
    /**
     * Default controller id for each device
     */
    const val Player1Device = 0
    const val Player2Device = 1
    const val Player3Device = 2
    const val Player4Device = 3
    const val Player5Device = 4
    const val Player6Device = 5
    const val Player7Device = 6
    const val Player8Device = 7
    const val ConsoleDevice = 8

    /**
     * Controller type for each device
     */
    const val ProController = 3
    const val Handheld = 4
    const val JoyconDual = 5
    const val JoyconLeft = 6
    const val JoyconRight = 7
    const val GameCube = 8
    const val Pokeball = 9
    const val NES = 10
    const val SNES = 11
    const val N64 = 12
    const val SegaGenesis = 13

    @JvmField
    var sEmulationActivity = WeakReference<EmulationActivity?>(null)

    init {
        try {
            System.loadLibrary("yuzu-android")
        } catch (ex: UnsatisfiedLinkError) {
            error("[NativeLibrary] $ex")
        }
    }

    @Keep
    @JvmStatic
    fun openContentUri(path: String?, openmode: String?): Int {
        return if (isNativePath(path!!)) {
            YuzuApplication.documentsTree!!.openContentUri(path, openmode)
        } else {
            openContentUri(appContext, path, openmode)
        }
    }

    @Keep
    @JvmStatic
    fun getSize(path: String?): Long {
        return if (isNativePath(path!!)) {
            YuzuApplication.documentsTree!!.getFileSize(path)
        } else {
            getFileSize(appContext, path)
        }
    }

    @Keep
    @JvmStatic
    fun exists(path: String?): Boolean {
        return if (isNativePath(path!!)) {
            YuzuApplication.documentsTree!!.exists(path)
        } else {
            exists(appContext, path)
        }
    }

    @Keep
    @JvmStatic
    fun isDirectory(path: String?): Boolean {
        return if (isNativePath(path!!)) {
            YuzuApplication.documentsTree!!.isDirectory(path)
        } else {
            isDirectory(appContext, path)
        }
    }

    /**
     * Returns true if pro controller isn't available and handheld is
     */
    external fun isHandheldOnly(): Boolean

    /**
     * Changes controller type for a specific device.
     *
     * @param Device The input descriptor of the gamepad.
     * @param Type The NpadStyleIndex of the gamepad.
     */
    external fun setDeviceType(Device: Int, Type: Int): Boolean

    /**
     * Handles event when a gamepad is connected.
     *
     * @param Device The input descriptor of the gamepad.
     */
    external fun onGamePadConnectEvent(Device: Int): Boolean

    /**
     * Handles event when a gamepad is disconnected.
     *
     * @param Device The input descriptor of the gamepad.
     */
    external fun onGamePadDisconnectEvent(Device: Int): Boolean

    /**
     * Handles button press events for a gamepad.
     *
     * @param Device The input descriptor of the gamepad.
     * @param Button Key code identifying which button was pressed.
     * @param Action Mask identifying which action is happening (button pressed down, or button released).
     * @return If we handled the button press.
     */
    external fun onGamePadButtonEvent(Device: Int, Button: Int, Action: Int): Boolean

    /**
     * Handles joystick movement events.
     *
     * @param Device The device ID of the gamepad.
     * @param Axis   The axis ID
     * @param x_axis The value of the x-axis represented by the given ID.
     * @param y_axis The value of the y-axis represented by the given ID.
     */
    external fun onGamePadJoystickEvent(
        Device: Int,
        Axis: Int,
        x_axis: Float,
        y_axis: Float
    ): Boolean

    /**
     * Handles motion events.
     *
     * @param delta_timestamp         The finger id corresponding to this event
     * @param gyro_x,gyro_y,gyro_z    The value of the accelerometer sensor.
     * @param accel_x,accel_y,accel_z The value of the y-axis
     */
    external fun onGamePadMotionEvent(
        Device: Int,
        delta_timestamp: Long,
        gyro_x: Float,
        gyro_y: Float,
        gyro_z: Float,
        accel_x: Float,
        accel_y: Float,
        accel_z: Float
    ): Boolean

    /**
     * Signals and load a nfc tag
     *
     * @param data         Byte array containing all the data from a nfc tag
     */
    external fun onReadNfcTag(data: ByteArray?): Boolean

    /**
     * Removes current loaded nfc tag
     */
    external fun onRemoveNfcTag(): Boolean

    /**
     * Handles touch press events.
     *
     * @param finger_id The finger id corresponding to this event
     * @param x_axis    The value of the x-axis.
     * @param y_axis    The value of the y-axis.
     */
    external fun onTouchPressed(finger_id: Int, x_axis: Float, y_axis: Float)

    /**
     * Handles touch movement.
     *
     * @param x_axis The value of the instantaneous x-axis.
     * @param y_axis The value of the instantaneous y-axis.
     */
    external fun onTouchMoved(finger_id: Int, x_axis: Float, y_axis: Float)

    /**
     * Handles touch release events.
     *
     * @param finger_id The finger id corresponding to this event
     */
    external fun onTouchReleased(finger_id: Int)

    external fun reloadSettings()

    external fun initGameIni(gameID: String?)

    /**
     * Gets the embedded icon within the given ROM.
     *
     * @param filename the file path to the ROM.
     * @return a byte array containing the JPEG data for the icon.
     */
    external fun getIcon(filename: String): ByteArray

    /**
     * Gets the embedded title of the given ISO/ROM.
     *
     * @param filename The file path to the ISO/ROM.
     * @return the embedded title of the ISO/ROM.
     */
    external fun getTitle(filename: String): String

    external fun getDescription(filename: String): String

    external fun getGameId(filename: String): String

    external fun getRegions(filename: String): String

    external fun getCompany(filename: String): String

    external fun isHomebrew(filename: String): Boolean

    external fun setAppDirectory(directory: String)

    external fun installFileToNand(filename: String): Int

    external fun initializeGpuDriver(
        hookLibDir: String?,
        customDriverDir: String?,
        customDriverName: String?,
        fileRedirectDir: String?
    )

    external fun reloadKeys(): Boolean

    external fun initializeEmulation()

    external fun defaultCPUCore(): Int

    /**
     * Begins emulation.
     */
    external fun run(path: String?)

    /**
     * Begins emulation from the specified savestate.
     */
    external fun run(path: String?, savestatePath: String?, deleteSavestate: Boolean)

    // Surface Handling
    external fun surfaceChanged(surf: Surface?)

    external fun surfaceDestroyed()

    /**
     * Unpauses emulation from a paused state.
     */
    external fun unpauseEmulation()

    /**
     * Pauses emulation.
     */
    external fun pauseEmulation()

    /**
     * Stops emulation.
     */
    external fun stopEmulation()

    /**
     * Resets the in-memory ROM metadata cache.
     */
    external fun resetRomMetadata()

    /**
     * Returns true if emulation is running (or is paused).
     */
    external fun isRunning(): Boolean

    /**
     * Returns true if emulation is paused.
     */
    external fun isPaused(): Boolean

    /**
     * Mutes emulation sound
     */
    external fun muteAudio(): Boolean

    /**
     * Unmutes emulation sound
     */
    external fun unmuteAudio(): Boolean

    /**
     * Returns true if emulation audio is muted.
     */
    external fun isMuted(): Boolean

    /**
     * Returns the performance stats for the current game
     */
    external fun getPerfStats(): DoubleArray

    /**
     * Notifies the core emulation that the orientation has changed.
     */
    external fun notifyOrientationChange(layout_option: Int, rotation: Int)

    enum class CoreError {
        ErrorSystemFiles,
        ErrorSavestate,
        ErrorUnknown
    }

    private var coreErrorAlertResult = false
    private val coreErrorAlertLock = Object()

    class CoreErrorDialogFragment : DialogFragment() {
        override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
            val title = requireArguments().serializable<String>("title")
            val message = requireArguments().serializable<String>("message")

            return MaterialAlertDialogBuilder(requireActivity())
                .setTitle(title)
                .setMessage(message)
                .setPositiveButton(R.string.continue_button, null)
                .setNegativeButton(R.string.abort_button) { _: DialogInterface?, _: Int ->
                    coreErrorAlertResult = false
                    synchronized(coreErrorAlertLock) { coreErrorAlertLock.notify() }
                }
                .create()
        }

        override fun onDismiss(dialog: DialogInterface) {
            coreErrorAlertResult = true
            synchronized(coreErrorAlertLock) { coreErrorAlertLock.notify() }
        }

        companion object {
            fun newInstance(title: String?, message: String?): CoreErrorDialogFragment {
                val frag = CoreErrorDialogFragment()
                val args = Bundle()
                args.putString("title", title)
                args.putString("message", message)
                frag.arguments = args
                return frag
            }
        }
    }

    private fun onCoreErrorImpl(title: String, message: String) {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            error("[NativeLibrary] EmulationActivity not present")
            return
        }

        val fragment = CoreErrorDialogFragment.newInstance(title, message)
        fragment.show(emulationActivity.supportFragmentManager, "coreError")
    }

    /**
     * Handles a core error.
     *
     * @return true: continue; false: abort
     */
    fun onCoreError(error: CoreError?, details: String): Boolean {
        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            error("[NativeLibrary] EmulationActivity not present")
            return false
        }

        val title: String
        val message: String
        when (error) {
            CoreError.ErrorSystemFiles -> {
                title = emulationActivity.getString(R.string.system_archive_not_found)
                message = emulationActivity.getString(
                    R.string.system_archive_not_found_message,
                    details.ifEmpty { emulationActivity.getString(R.string.system_archive_general) }
                )
            }

            CoreError.ErrorSavestate -> {
                title = emulationActivity.getString(R.string.save_load_error)
                message = details
            }

            CoreError.ErrorUnknown -> {
                title = emulationActivity.getString(R.string.fatal_error)
                message = emulationActivity.getString(R.string.fatal_error_message)
            }

            else -> {
                return true
            }
        }

        // Show the AlertDialog on the main thread.
        emulationActivity.runOnUiThread(Runnable { onCoreErrorImpl(title, message) })

        // Wait for the lock to notify that it is complete.
        synchronized(coreErrorAlertLock) { coreErrorAlertLock.wait() }

        return coreErrorAlertResult
    }

    @Keep
    @JvmStatic
    fun exitEmulationActivity(resultCode: Int) {
        val Success = 0
        val ErrorNotInitialized = 1
        val ErrorGetLoader = 2
        val ErrorSystemFiles = 3
        val ErrorSharedFont = 4
        val ErrorVideoCore = 5
        val ErrorUnknown = 6
        val ErrorLoader = 7

        val captionId: Int
        var descriptionId: Int
        when (resultCode) {
            ErrorVideoCore -> {
                captionId = R.string.loader_error_video_core
                descriptionId = R.string.loader_error_video_core_description
            }

            else -> {
                captionId = R.string.loader_error_encrypted
                descriptionId = R.string.loader_error_encrypted_roms_description
                if (!reloadKeys()) {
                    descriptionId = R.string.loader_error_encrypted_keys_description
                }
            }
        }

        val emulationActivity = sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null, can't exit.")
            return
        }

        val builder = MaterialAlertDialogBuilder(emulationActivity)
            .setTitle(captionId)
            .setMessage(
                Html.fromHtml(
                    emulationActivity.getString(descriptionId),
                    Html.FROM_HTML_MODE_LEGACY
                )
            )
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                emulationActivity.finish()
            }
            .setOnDismissListener { emulationActivity.finish() }
        emulationActivity.runOnUiThread {
            val alert = builder.create()
            alert.show()
            (alert.findViewById<View>(android.R.id.message) as TextView).movementMethod =
                LinkMovementMethod.getInstance()
        }
    }

    fun setEmulationActivity(emulationActivity: EmulationActivity?) {
        Log.verbose("[NativeLibrary] Registering EmulationActivity.")
        sEmulationActivity = WeakReference(emulationActivity)
    }

    fun clearEmulationActivity() {
        Log.verbose("[NativeLibrary] Unregistering EmulationActivity.")
        sEmulationActivity.clear()
    }

    @Keep
    @JvmStatic
    fun onEmulationStarted() {
        sEmulationActivity.get()!!.onEmulationStarted()
    }

    @Keep
    @JvmStatic
    fun onEmulationStopped(status: Int) {
        sEmulationActivity.get()!!.onEmulationStopped(status)
    }

    /**
     * Logs the Yuzu version, Android version and, CPU.
     */
    external fun logDeviceInfo()

    /**
     * Submits inline keyboard text. Called on input for buttons that result text.
     * @param text Text to submit to the inline software keyboard implementation.
     */
    external fun submitInlineKeyboardText(text: String?)

    /**
     * Submits inline keyboard input. Used to indicate keys pressed that are not text.
     * @param key_code Android Key Code associated with the keyboard input.
     */
    external fun submitInlineKeyboardInput(key_code: Int)

    /**
     * Button type for use in onTouchEvent
     */
    object ButtonType {
        const val BUTTON_A = 0
        const val BUTTON_B = 1
        const val BUTTON_X = 2
        const val BUTTON_Y = 3
        const val STICK_L = 4
        const val STICK_R = 5
        const val TRIGGER_L = 6
        const val TRIGGER_R = 7
        const val TRIGGER_ZL = 8
        const val TRIGGER_ZR = 9
        const val BUTTON_PLUS = 10
        const val BUTTON_MINUS = 11
        const val DPAD_LEFT = 12
        const val DPAD_UP = 13
        const val DPAD_RIGHT = 14
        const val DPAD_DOWN = 15
        const val BUTTON_SL = 16
        const val BUTTON_SR = 17
        const val BUTTON_HOME = 18
        const val BUTTON_CAPTURE = 19
    }

    /**
     * Stick type for use in onTouchEvent
     */
    object StickType {
        const val STICK_L = 0
        const val STICK_R = 1
    }

    /**
     * Button states
     */
    object ButtonState {
        const val RELEASED = 0
        const val PRESSED = 1
    }

    /**
     * Result from installFileToNand
     */
    object InstallFileToNandResult {
        const val Success = 0
        const val SuccessFileOverwritten = 1
        const val Error = 2
        const val ErrorBaseGame = 3
        const val ErrorFilenameExtension = 4
    }
}
