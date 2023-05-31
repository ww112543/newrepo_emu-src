// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.overlay

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.graphics.drawable.VectorDrawable
import android.os.Build
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.View
import android.view.View.OnTouchListener
import android.view.WindowInsets
import androidx.core.content.ContextCompat
import androidx.preference.PreferenceManager
import androidx.window.layout.WindowMetricsCalculator
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.NativeLibrary.ButtonType
import org.yuzu.yuzu_emu.NativeLibrary.StickType
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings
import kotlin.math.max
import kotlin.math.min

/**
 * Draws the interactive input overlay on top of the
 * [SurfaceView] that is rendering emulation.
 */
class InputOverlay(context: Context, attrs: AttributeSet?) : SurfaceView(context, attrs),
    OnTouchListener {
    private val overlayButtons: MutableSet<InputOverlayDrawableButton> = HashSet()
    private val overlayDpads: MutableSet<InputOverlayDrawableDpad> = HashSet()
    private val overlayJoysticks: MutableSet<InputOverlayDrawableJoystick> = HashSet()

    private var inEditMode = false
    private var buttonBeingConfigured: InputOverlayDrawableButton? = null
    private var dpadBeingConfigured: InputOverlayDrawableDpad? = null
    private var joystickBeingConfigured: InputOverlayDrawableJoystick? = null

    private val preferences: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    private lateinit var windowInsets: WindowInsets

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)

        windowInsets = rootWindowInsets

        if (!preferences.getBoolean(Settings.PREF_OVERLAY_INIT, false)) {
            defaultOverlay()
        }

        // Load the controls.
        refreshControls()

        // Set the on touch listener.
        setOnTouchListener(this)

        // Force draw
        setWillNotDraw(false)

        // Request focus for the overlay so it has priority on presses.
        requestFocus()
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)
        for (button in overlayButtons) {
            button.draw(canvas)
        }
        for (dpad in overlayDpads) {
            dpad.draw(canvas)
        }
        for (joystick in overlayJoysticks) {
            joystick.draw(canvas)
        }
    }

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        if (inEditMode) {
            return onTouchWhileEditing(event)
        }

        var shouldUpdateView = false
        val playerIndex =
            if (NativeLibrary.isHandheldOnly()) NativeLibrary.ConsoleDevice else NativeLibrary.Player1Device

        for (button in overlayButtons) {
            if (!button.updateStatus(event)) {
                continue
            }
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                button.buttonId,
                button.status
            )
            playHaptics(event)
            shouldUpdateView = true
        }

        for (dpad in overlayDpads) {
            if (!dpad.updateStatus(event, EmulationMenuSettings.dpadSlide)) {
                continue
            }
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.upId,
                dpad.upStatus
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.downId,
                dpad.downStatus
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.leftId,
                dpad.leftStatus
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.rightId,
                dpad.rightStatus
            )
            playHaptics(event)
            shouldUpdateView = true
        }

        for (joystick in overlayJoysticks) {
            if (!joystick.updateStatus(event)) {
                continue
            }
            val axisID = joystick.joystickId
            NativeLibrary.onGamePadJoystickEvent(
                playerIndex,
                axisID,
                joystick.xAxis,
                joystick.realYAxis
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                joystick.buttonId,
                joystick.buttonStatus
            )
            playHaptics(event)
            shouldUpdateView = true
        }

        if (shouldUpdateView)
            invalidate()

        if (!preferences.getBoolean(Settings.PREF_TOUCH_ENABLED, true)) {
            return true
        }

        val pointerIndex = event.actionIndex
        val xPosition = event.getX(pointerIndex).toInt()
        val yPosition = event.getY(pointerIndex).toInt()
        val pointerId = event.getPointerId(pointerIndex)
        val motionEvent = event.action and MotionEvent.ACTION_MASK
        val isActionDown =
            motionEvent == MotionEvent.ACTION_DOWN || motionEvent == MotionEvent.ACTION_POINTER_DOWN
        val isActionMove = motionEvent == MotionEvent.ACTION_MOVE
        val isActionUp =
            motionEvent == MotionEvent.ACTION_UP || motionEvent == MotionEvent.ACTION_POINTER_UP

        if (isActionDown && !isTouchInputConsumed(pointerId)) {
            NativeLibrary.onTouchPressed(pointerId, xPosition.toFloat(), yPosition.toFloat())
        }

        if (isActionMove) {
            for (i in 0 until event.pointerCount) {
                val fingerId = event.getPointerId(i)
                if (isTouchInputConsumed(fingerId)) {
                    continue
                }
                NativeLibrary.onTouchMoved(fingerId, event.getX(i), event.getY(i))
            }
        }

        if (isActionUp && !isTouchInputConsumed(pointerId)) {
            NativeLibrary.onTouchReleased(pointerId)
        }

        return true
    }

    private fun playHaptics(event: MotionEvent) {
        if (EmulationMenuSettings.hapticFeedback) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP ->
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY_RELEASE)
            }
        }
    }

    private fun isTouchInputConsumed(track_id: Int): Boolean {
        for (button in overlayButtons) {
            if (button.trackId == track_id) {
                return true
            }
        }
        for (dpad in overlayDpads) {
            if (dpad.trackId == track_id) {
                return true
            }
        }
        for (joystick in overlayJoysticks) {
            if (joystick.trackId == track_id) {
                return true
            }
        }
        return false
    }

    private fun onTouchWhileEditing(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val fingerPositionX = event.getX(pointerIndex).toInt()
        val fingerPositionY = event.getY(pointerIndex).toInt()

        // TODO: Provide support for portrait layout
        //val orientation =
        //    if (resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT) "-Portrait" else ""

        for (button in overlayButtons) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    // If no button is being moved now, remember the currently touched button to move.
                    if (buttonBeingConfigured == null &&
                        button.bounds.contains(
                            fingerPositionX,
                            fingerPositionY
                        )
                    ) {
                        buttonBeingConfigured = button
                        buttonBeingConfigured!!.onConfigureTouch(event)
                    }

                MotionEvent.ACTION_MOVE -> if (buttonBeingConfigured != null) {
                    buttonBeingConfigured!!.onConfigureTouch(event)
                    invalidate()
                    return true
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (buttonBeingConfigured === button) {
                    // Persist button position by saving new place.
                    saveControlPosition(
                        buttonBeingConfigured!!.buttonId,
                        buttonBeingConfigured!!.bounds.centerX(),
                        buttonBeingConfigured!!.bounds.centerY(),
                        ""
                    )
                    buttonBeingConfigured = null
                }
            }
        }

        for (dpad in overlayDpads) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    // If no button is being moved now, remember the currently touched button to move.
                    if (buttonBeingConfigured == null &&
                        dpad.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        dpadBeingConfigured = dpad
                        dpadBeingConfigured!!.onConfigureTouch(event)
                    }

                MotionEvent.ACTION_MOVE -> if (dpadBeingConfigured != null) {
                    dpadBeingConfigured!!.onConfigureTouch(event)
                    invalidate()
                    return true
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (dpadBeingConfigured === dpad) {
                    // Persist button position by saving new place.
                    saveControlPosition(
                        dpadBeingConfigured!!.upId,
                        dpadBeingConfigured!!.bounds.centerX(),
                        dpadBeingConfigured!!.bounds.centerY(),
                        ""
                    )
                    dpadBeingConfigured = null
                }
            }
        }

        for (joystick in overlayJoysticks) {
            when (event.action) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> if (joystickBeingConfigured == null &&
                    joystick.bounds.contains(
                        fingerPositionX,
                        fingerPositionY
                    )
                ) {
                    joystickBeingConfigured = joystick
                    joystickBeingConfigured!!.onConfigureTouch(event)
                }

                MotionEvent.ACTION_MOVE -> if (joystickBeingConfigured != null) {
                    joystickBeingConfigured!!.onConfigureTouch(event)
                    invalidate()
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (joystickBeingConfigured != null) {
                    saveControlPosition(
                        joystickBeingConfigured!!.buttonId,
                        joystickBeingConfigured!!.bounds.centerX(),
                        joystickBeingConfigured!!.bounds.centerY(),
                        ""
                    )
                    joystickBeingConfigured = null
                }
            }
        }

        return true
    }

    private fun addOverlayControls(orientation: String) {
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_0, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_a,
                    R.drawable.facebutton_a_depressed,
                    ButtonType.BUTTON_A,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_1, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_b,
                    R.drawable.facebutton_b_depressed,
                    ButtonType.BUTTON_B,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_2, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_x,
                    R.drawable.facebutton_x_depressed,
                    ButtonType.BUTTON_X,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_3, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_y,
                    R.drawable.facebutton_y_depressed,
                    ButtonType.BUTTON_Y,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_4, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.l_shoulder,
                    R.drawable.l_shoulder_depressed,
                    ButtonType.TRIGGER_L,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_5, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.r_shoulder,
                    R.drawable.r_shoulder_depressed,
                    ButtonType.TRIGGER_R,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_6, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.zl_trigger,
                    R.drawable.zl_trigger_depressed,
                    ButtonType.TRIGGER_ZL,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_7, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.zr_trigger,
                    R.drawable.zr_trigger_depressed,
                    ButtonType.TRIGGER_ZR,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_8, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_plus,
                    R.drawable.facebutton_plus_depressed,
                    ButtonType.BUTTON_PLUS,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_9, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_minus,
                    R.drawable.facebutton_minus_depressed,
                    ButtonType.BUTTON_MINUS,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_10, true)) {
            overlayDpads.add(
                initializeOverlayDpad(
                    context,
                    R.drawable.dpad_standard,
                    R.drawable.dpad_standard_cardinal_depressed,
                    R.drawable.dpad_standard_diagonal_depressed,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_11, true)) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.joystick_range,
                    R.drawable.joystick,
                    R.drawable.joystick_depressed,
                    StickType.STICK_L,
                    ButtonType.STICK_L,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_12, true)) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    R.drawable.joystick_range,
                    R.drawable.joystick,
                    R.drawable.joystick_depressed,
                    StickType.STICK_R,
                    ButtonType.STICK_R,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_13, false)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_home,
                    R.drawable.facebutton_home_depressed,
                    ButtonType.BUTTON_HOME,
                    orientation
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_TOGGLE_14, false)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    R.drawable.facebutton_screenshot,
                    R.drawable.facebutton_screenshot_depressed,
                    ButtonType.BUTTON_CAPTURE,
                    orientation
                )
            )
        }
    }

    fun refreshControls() {
        // Remove all the overlay buttons from the HashSet.
        overlayButtons.clear()
        overlayDpads.clear()
        overlayJoysticks.clear()
        val orientation =
            if (resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT) "-Portrait" else ""

        // Add all the enabled overlay items back to the HashSet.
        if (EmulationMenuSettings.showOverlay) {
            addOverlayControls(orientation)
        }
        invalidate()
    }

    private fun saveControlPosition(sharedPrefsId: Int, x: Int, y: Int, orientation: String) {
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext).edit()
            .putFloat("$sharedPrefsId$orientation-X", x.toFloat())
            .putFloat("$sharedPrefsId$orientation-Y", y.toFloat())
            .apply()
    }

    fun setIsInEditMode(editMode: Boolean) {
        inEditMode = editMode
    }

    private fun defaultOverlay() {
        if (!preferences.getBoolean(Settings.PREF_OVERLAY_INIT, false)) {
            defaultOverlayLandscape()
        }

        resetButtonPlacement()
        preferences.edit()
            .putBoolean(Settings.PREF_OVERLAY_INIT, true)
            .apply()
    }

    fun resetButtonPlacement() {
        defaultOverlayLandscape()
        refreshControls()
    }

    private fun defaultOverlayLandscape() {
        // Get screen size
        val windowMetrics =
            WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(context as Activity)
        var maxY = windowMetrics.bounds.height().toFloat()
        var maxX = windowMetrics.bounds.width().toFloat()
        var minY = 0
        var minX = 0

        // If we have API access, calculate the safe area to draw the overlay
        var cutoutLeft = 0
        var cutoutBottom = 0
        val insets = windowInsets.displayCutout
        if (insets != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            maxY =
                if (insets.boundingRectTop.bottom != 0) insets.boundingRectTop.bottom.toFloat() else maxY
            maxX =
                if (insets.boundingRectRight.left != 0) insets.boundingRectRight.left.toFloat() else maxX
            minX = insets.boundingRectLeft.right - insets.boundingRectLeft.left
            minY = insets.boundingRectBottom.top - insets.boundingRectBottom.bottom

            cutoutLeft = insets.boundingRectRight.right - insets.boundingRectRight.left
            cutoutBottom = insets.boundingRectTop.top - insets.boundingRectTop.bottom
        }

        // This makes sure that if we have an inset on one side of the screen, we mirror it on
        // the other side. Since removing space from one of the max values messes with the scale,
        // we also have to account for it using our min values.
        if (maxX.toInt() != windowMetrics.bounds.width()) minX += cutoutLeft
        if (maxY.toInt() != windowMetrics.bounds.height()) minY += cutoutBottom
        if (minX > 0 && maxX.toInt() == windowMetrics.bounds.width()) {
            maxX -= (minX * 2)
        } else if (minX > 0) {
            maxX -= minX
        }
        if (minY > 0 && maxY.toInt() == windowMetrics.bounds.height()) {
            maxY -= (minY * 2)
        } else if (minY > 0) {
            maxY -= minY
        }

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        preferences.edit()
            .putFloat(
                ButtonType.BUTTON_A.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_A_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_A.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_A_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_B.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_B_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_B.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_B_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_X.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_X_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_X.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_X_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_Y.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_Y_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_Y.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_Y_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.TRIGGER_ZL.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_TRIGGER_ZL_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.TRIGGER_ZL.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_TRIGGER_ZL_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.TRIGGER_ZR.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_TRIGGER_ZR_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.TRIGGER_ZR.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_TRIGGER_ZR_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.DPAD_UP.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_DPAD_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.DPAD_UP.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_DPAD_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.TRIGGER_L.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_TRIGGER_L_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.TRIGGER_L.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_TRIGGER_L_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.TRIGGER_R.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_TRIGGER_R_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.TRIGGER_R.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_TRIGGER_R_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_PLUS.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_PLUS_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_PLUS.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_PLUS_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_MINUS.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_MINUS_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_MINUS.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_MINUS_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_HOME.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_HOME_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_HOME.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_HOME_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.BUTTON_CAPTURE.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_BUTTON_CAPTURE_X)
                    .toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.BUTTON_CAPTURE.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_BUTTON_CAPTURE_Y)
                    .toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.STICK_R.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_STICK_R_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.STICK_R.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_STICK_R_Y).toFloat() / 1000 * maxY + minY
            )
            .putFloat(
                ButtonType.STICK_L.toString() + "-X",
                resources.getInteger(R.integer.SWITCH_STICK_L_X).toFloat() / 1000 * maxX + minX
            )
            .putFloat(
                ButtonType.STICK_L.toString() + "-Y",
                resources.getInteger(R.integer.SWITCH_STICK_L_Y).toFloat() / 1000 * maxY + minY
            )
            .apply()
    }

    override fun isInEditMode(): Boolean {
        return inEditMode
    }

    companion object {
        /**
         * Resizes a [Bitmap] by a given scale factor
         *
         * @param context       Context for getting the vector drawable
         * @param drawableId    The ID of the drawable to scale.
         * @param scale         The scale factor for the bitmap.
         * @return              The scaled [Bitmap]
         */
        private fun getBitmap(context: Context, drawableId: Int, scale: Float): Bitmap {
            val vectorDrawable = ContextCompat.getDrawable(context, drawableId) as VectorDrawable

            val bitmap = Bitmap.createBitmap(
                (vectorDrawable.intrinsicWidth * scale).toInt(),
                (vectorDrawable.intrinsicHeight * scale).toInt(),
                Bitmap.Config.ARGB_8888
            )

            val dm = context.resources.displayMetrics
            val minScreenDimension = min(dm.widthPixels, dm.heightPixels)

            val maxBitmapDimension = max(bitmap.width, bitmap.height)
            val bitmapScale = scale * minScreenDimension / maxBitmapDimension

            val scaledBitmap = Bitmap.createScaledBitmap(
                bitmap,
                (bitmap.width * bitmapScale).toInt(),
                (bitmap.height * bitmapScale).toInt(),
                true
            )

            val canvas = Canvas(scaledBitmap)
            vectorDrawable.setBounds(0, 0, canvas.width, canvas.height)
            vectorDrawable.draw(canvas)
            return scaledBitmap
        }

        /**
         * Initializes an InputOverlayDrawableButton, given by resId, with all of the
         * parameters set for it to be properly shown on the InputOverlay.
         *
         *
         * This works due to the way the X and Y coordinates are stored within
         * the [SharedPreferences].
         *
         *
         * In the input overlay configuration menu,
         * once a touch event begins and then ends (ie. Organizing the buttons to one's own liking for the overlay).
         * the X and Y coordinates of the button at the END of its touch event
         * (when you remove your finger/stylus from the touchscreen) are then stored
         * within a SharedPreferences instance so that those values can be retrieved here.
         *
         *
         * This has a few benefits over the conventional way of storing the values
         * (ie. within the yuzu ini file).
         *
         *  * No native calls
         *  * Keeps Android-only values inside the Android environment
         *
         *
         *
         * Technically no modifications should need to be performed on the returned
         * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
         * for Android to call the onDraw method.
         *
         * @param context      The current [Context].
         * @param defaultResId The resource ID of the [Drawable] to get the [Bitmap] of (Default State).
         * @param pressedResId The resource ID of the [Drawable] to get the [Bitmap] of (Pressed State).
         * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
         * @return An [InputOverlayDrawableButton] with the correct drawing bounds set.
         */
        private fun initializeOverlayButton(
            context: Context,
            defaultResId: Int,
            pressedResId: Int,
            buttonId: Int,
            orientation: String
        ): InputOverlayDrawableButton {
            // Resources handle for fetching the initial Drawable resource.
            val res = context.resources

            // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
            val sPrefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

            // Decide scale based on button ID and user preference
            var scale: Float = when (buttonId) {
                ButtonType.BUTTON_HOME,
                ButtonType.BUTTON_CAPTURE,
                ButtonType.BUTTON_PLUS,
                ButtonType.BUTTON_MINUS -> 0.07f

                ButtonType.TRIGGER_L,
                ButtonType.TRIGGER_R,
                ButtonType.TRIGGER_ZL,
                ButtonType.TRIGGER_ZR -> 0.26f

                else -> 0.11f
            }
            scale *= (sPrefs.getInt(Settings.PREF_CONTROL_SCALE, 50) + 50).toFloat()
            scale /= 100f

            // Initialize the InputOverlayDrawableButton.
            val defaultStateBitmap = getBitmap(context, defaultResId, scale)
            val pressedStateBitmap = getBitmap(context, pressedResId, scale)
            val overlayDrawable =
                InputOverlayDrawableButton(res, defaultStateBitmap, pressedStateBitmap, buttonId)

            // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
            // These were set in the input overlay configuration menu.
            val xKey = "$buttonId$orientation-X"
            val yKey = "$buttonId$orientation-Y"
            val drawableX = sPrefs.getFloat(xKey, 0f).toInt()
            val drawableY = sPrefs.getFloat(yKey, 0f).toInt()
            val width = overlayDrawable.width
            val height = overlayDrawable.height

            // Now set the bounds for the InputOverlayDrawableButton.
            // This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
            overlayDrawable.setBounds(
                drawableX - (width / 2),
                drawableY - (height / 2),
                drawableX + (width / 2),
                drawableY + (height / 2)
            )

            // Need to set the image's position
            overlayDrawable.setPosition(
                drawableX - (width / 2),
                drawableY - (height / 2)
            )
            return overlayDrawable
        }

        /**
         * Initializes an [InputOverlayDrawableDpad]
         *
         * @param context                   The current [Context].
         * @param defaultResId              The [Bitmap] resource ID of the default state.
         * @param pressedOneDirectionResId  The [Bitmap] resource ID of the pressed state in one direction.
         * @param pressedTwoDirectionsResId The [Bitmap] resource ID of the pressed state in two directions.
         * @return the initialized [InputOverlayDrawableDpad]
         */
        private fun initializeOverlayDpad(
            context: Context,
            defaultResId: Int,
            pressedOneDirectionResId: Int,
            pressedTwoDirectionsResId: Int,
            orientation: String
        ): InputOverlayDrawableDpad {
            // Resources handle for fetching the initial Drawable resource.
            val res = context.resources

            // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
            val sPrefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

            // Decide scale based on button ID and user preference
            var scale = 0.25f
            scale *= (sPrefs.getInt(Settings.PREF_CONTROL_SCALE, 50) + 50).toFloat()
            scale /= 100f

            // Initialize the InputOverlayDrawableDpad.
            val defaultStateBitmap =
                getBitmap(context, defaultResId, scale)
            val pressedOneDirectionStateBitmap = getBitmap(context, pressedOneDirectionResId, scale)
            val pressedTwoDirectionsStateBitmap =
                getBitmap(context, pressedTwoDirectionsResId, scale)

            val overlayDrawable = InputOverlayDrawableDpad(
                res,
                defaultStateBitmap,
                pressedOneDirectionStateBitmap,
                pressedTwoDirectionsStateBitmap,
                ButtonType.DPAD_UP,
                ButtonType.DPAD_DOWN,
                ButtonType.DPAD_LEFT,
                ButtonType.DPAD_RIGHT
            )

            // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
            // These were set in the input overlay configuration menu.
            val drawableX = sPrefs.getFloat("${ButtonType.DPAD_UP}$orientation-X", 0f).toInt()
            val drawableY = sPrefs.getFloat("${ButtonType.DPAD_UP}$orientation-Y", 0f).toInt()
            val width = overlayDrawable.width
            val height = overlayDrawable.height

            // Now set the bounds for the InputOverlayDrawableDpad.
            // This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
            overlayDrawable.setBounds(
                drawableX - (width / 2),
                drawableY - (height / 2),
                drawableX + (width / 2),
                drawableY + (height / 2)
            )

            // Need to set the image's position
            overlayDrawable.setPosition(drawableX - (width / 2), drawableY - (height / 2))
            return overlayDrawable
        }

        /**
         * Initializes an [InputOverlayDrawableJoystick]
         *
         * @param context         The current [Context]
         * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
         * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
         * @param pressedResInner Resource ID for the pressed inner image of the joystick.
         * @param joystick        Identifier for which joystick this is.
         * @param button          Identifier for which joystick button this is.
         * @return the initialized [InputOverlayDrawableJoystick].
         */
        private fun initializeOverlayJoystick(
            context: Context,
            resOuter: Int,
            defaultResInner: Int,
            pressedResInner: Int,
            joystick: Int,
            button: Int,
            orientation: String
        ): InputOverlayDrawableJoystick {
            // Resources handle for fetching the initial Drawable resource.
            val res = context.resources

            // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
            val sPrefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

            // Decide scale based on user preference
            var scale = 0.3f
            scale *= (sPrefs.getInt(Settings.PREF_CONTROL_SCALE, 50) + 50).toFloat()
            scale /= 100f

            // Initialize the InputOverlayDrawableJoystick.
            val bitmapOuter = getBitmap(context, resOuter, scale)
            val bitmapInnerDefault = getBitmap(context, defaultResInner, 1.0f)
            val bitmapInnerPressed = getBitmap(context, pressedResInner, 1.0f)

            // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
            // These were set in the input overlay configuration menu.
            val drawableX = sPrefs.getFloat("$button$orientation-X", 0f).toInt()
            val drawableY = sPrefs.getFloat("$button$orientation-Y", 0f).toInt()
            val outerScale = 1.66f

            // Now set the bounds for the InputOverlayDrawableJoystick.
            // This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
            val outerSize = bitmapOuter.width
            val outerRect = Rect(
                drawableX - (outerSize / 2),
                drawableY - (outerSize / 2),
                drawableX + (outerSize / 2),
                drawableY + (outerSize / 2)
            )
            val innerRect =
                Rect(0, 0, (outerSize / outerScale).toInt(), (outerSize / outerScale).toInt())

            // Send the drawableId to the joystick so it can be referenced when saving control position.
            val overlayDrawable = InputOverlayDrawableJoystick(
                res,
                bitmapOuter,
                bitmapInnerDefault,
                bitmapInnerPressed,
                outerRect,
                innerRect,
                joystick,
                button
            )

            // Need to set the image's position
            overlayDrawable.setPosition(drawableX, drawableY)
            return overlayDrawable
        }
    }
}
