// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.annotation.SuppressLint
import android.app.AlertDialog
import android.content.Context
import android.content.DialogInterface
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.*
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.widget.PopupMenu
import androidx.core.content.res.ResourcesCompat
import androidx.core.graphics.Insets
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.drawerlayout.widget.DrawerLayout
import androidx.drawerlayout.widget.DrawerLayout.DrawerListener
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowInfoTracker
import androidx.window.layout.WindowLayoutInfo
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.HomeNavigationDirections
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.DialogOverlayAdjustBinding
import org.yuzu.yuzu_emu.databinding.FragmentEmulationBinding
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.Settings.EmulationOrientation
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.EmulationViewModel
import org.yuzu.yuzu_emu.overlay.model.OverlayControl
import org.yuzu.yuzu_emu.overlay.model.OverlayLayout
import org.yuzu.yuzu_emu.utils.*
import java.lang.NullPointerException

class EmulationFragment : Fragment(), SurfaceHolder.Callback {
    private lateinit var emulationState: EmulationState
    private var emulationActivity: EmulationActivity? = null
    private var perfStatsUpdater: (() -> Unit)? = null

    private var _binding: FragmentEmulationBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<EmulationFragmentArgs>()

    private lateinit var game: Game

    private val emulationViewModel: EmulationViewModel by activityViewModels()
    private val driverViewModel: DriverViewModel by activityViewModels()

    private var isInFoldableLayout = false

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is EmulationActivity) {
            emulationActivity = context
            NativeLibrary.setEmulationActivity(context)

            lifecycleScope.launch(Dispatchers.Main) {
                lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                    WindowInfoTracker.getOrCreate(context)
                        .windowLayoutInfo(context)
                        .collect { updateFoldableLayout(context, it) }
                }
            }
        } else {
            throw IllegalStateException("EmulationFragment must have EmulationActivity parent")
        }
    }

    /**
     * Initialize anything that doesn't depend on the layout / views in here.
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        updateOrientation()

        val intentUri: Uri? = requireActivity().intent.data
        var intentGame: Game? = null
        if (intentUri != null) {
            intentGame = if (Game.extensions.contains(FileUtil.getExtension(intentUri))) {
                GameHelper.getGame(requireActivity().intent.data!!, false)
            } else {
                null
            }
        }

        try {
            game = if (args.game != null) {
                args.game!!
            } else {
                intentGame!!
            }
        } catch (e: NullPointerException) {
            Toast.makeText(
                requireContext(),
                R.string.no_game_present,
                Toast.LENGTH_SHORT
            ).show()
            requireActivity().finish()
            return
        }

        // Always load custom settings when launching a game from an intent
        if (args.custom || intentGame != null) {
            SettingsFile.loadCustomConfig(game)
            NativeConfig.unloadPerGameConfig()
        } else {
            NativeConfig.reloadGlobalConfig()
        }

        // Install the selected driver asynchronously as the game starts
        driverViewModel.onLaunchGame()

        // So this fragment doesn't restart on configuration changes; i.e. rotation.
        retainInstance = true
        emulationState = EmulationState(game.path) {
            return@EmulationState driverViewModel.isInteractionAllowed.value
        }
    }

    /**
     * Initialize the UI and start emulation in here.
     */
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentEmulationBinding.inflate(layoutInflater)
        return binding.root
    }

    // This is using the correct scope, lint is just acting up
    @SuppressLint("UnsafeRepeatOnLifecycleDetector")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (requireActivity().isFinishing) {
            return
        }

        binding.surfaceEmulation.holder.addCallback(this)
        binding.doneControlConfig.setOnClickListener { stopConfiguringControls() }

        binding.drawerLayout.addDrawerListener(object : DrawerListener {
            override fun onDrawerSlide(drawerView: View, slideOffset: Float) {
                binding.surfaceInputOverlay.dispatchTouchEvent(
                    MotionEvent.obtain(
                        SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis() + 100,
                        MotionEvent.ACTION_UP,
                        0f,
                        0f,
                        0
                    )
                )
            }

            override fun onDrawerOpened(drawerView: View) {
                binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_UNLOCKED)
            }

            override fun onDrawerClosed(drawerView: View) {
                binding.drawerLayout.setDrawerLockMode(IntSetting.LOCK_DRAWER.getInt())
            }

            override fun onDrawerStateChanged(newState: Int) {
                // No op
            }
        })
        binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED)
        binding.inGameMenu.getHeaderView(0).findViewById<TextView>(R.id.text_game_title).text =
            game.title

        binding.inGameMenu.menu.findItem(R.id.menu_lock_drawer).apply {
            val lockMode = IntSetting.LOCK_DRAWER.getInt()
            val titleId = if (lockMode == DrawerLayout.LOCK_MODE_LOCKED_CLOSED) {
                R.string.unlock_drawer
            } else {
                R.string.lock_drawer
            }
            val iconId = if (lockMode == DrawerLayout.LOCK_MODE_UNLOCKED) {
                R.drawable.ic_unlock
            } else {
                R.drawable.ic_lock
            }

            title = getString(titleId)
            icon = ResourcesCompat.getDrawable(
                resources,
                iconId,
                requireContext().theme
            )
        }

        binding.inGameMenu.setNavigationItemSelectedListener {
            when (it.itemId) {
                R.id.menu_pause_emulation -> {
                    if (emulationState.isPaused) {
                        emulationState.run(false)
                        it.title = resources.getString(R.string.emulation_pause)
                        it.icon = ResourcesCompat.getDrawable(
                            resources,
                            R.drawable.ic_pause,
                            requireContext().theme
                        )
                    } else {
                        emulationState.pause()
                        it.title = resources.getString(R.string.emulation_unpause)
                        it.icon = ResourcesCompat.getDrawable(
                            resources,
                            R.drawable.ic_play,
                            requireContext().theme
                        )
                    }
                    true
                }

                R.id.menu_settings -> {
                    val action = HomeNavigationDirections.actionGlobalSettingsActivity(
                        null,
                        Settings.MenuTag.SECTION_ROOT
                    )
                    binding.root.findNavController().navigate(action)
                    true
                }

                R.id.menu_settings_per_game -> {
                    val action = HomeNavigationDirections.actionGlobalSettingsActivity(
                        args.game,
                        Settings.MenuTag.SECTION_ROOT
                    )
                    binding.root.findNavController().navigate(action)
                    true
                }

                R.id.menu_overlay_controls -> {
                    showOverlayOptions()
                    true
                }

                R.id.menu_lock_drawer -> {
                    when (IntSetting.LOCK_DRAWER.getInt()) {
                        DrawerLayout.LOCK_MODE_UNLOCKED -> {
                            IntSetting.LOCK_DRAWER.setInt(DrawerLayout.LOCK_MODE_LOCKED_CLOSED)
                            it.title = resources.getString(R.string.unlock_drawer)
                            it.icon = ResourcesCompat.getDrawable(
                                resources,
                                R.drawable.ic_lock,
                                requireContext().theme
                            )
                        }

                        DrawerLayout.LOCK_MODE_LOCKED_CLOSED -> {
                            IntSetting.LOCK_DRAWER.setInt(DrawerLayout.LOCK_MODE_UNLOCKED)
                            it.title = resources.getString(R.string.lock_drawer)
                            it.icon = ResourcesCompat.getDrawable(
                                resources,
                                R.drawable.ic_unlock,
                                requireContext().theme
                            )
                        }
                    }
                    NativeConfig.saveGlobalConfig()
                    true
                }

                R.id.menu_exit -> {
                    emulationState.stop()
                    emulationViewModel.setIsEmulationStopping(true)
                    binding.drawerLayout.close()
                    binding.drawerLayout.setDrawerLockMode(DrawerLayout.LOCK_MODE_LOCKED_CLOSED)
                    true
                }

                else -> true
            }
        }

        setInsets()

        requireActivity().onBackPressedDispatcher.addCallback(
            requireActivity(),
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() {
                    if (!NativeLibrary.isRunning()) {
                        return
                    }

                    if (binding.drawerLayout.isOpen) {
                        binding.drawerLayout.close()
                    } else {
                        binding.drawerLayout.open()
                    }
                }
            }
        )

        GameIconUtils.loadGameIcon(game, binding.loadingImage)
        binding.loadingTitle.text = game.title
        binding.loadingTitle.isSelected = true
        binding.loadingText.isSelected = true

        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.STARTED) {
                    WindowInfoTracker.getOrCreate(requireContext())
                        .windowLayoutInfo(requireActivity())
                        .collect {
                            updateFoldableLayout(requireActivity() as EmulationActivity, it)
                        }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    emulationViewModel.shaderProgress.collectLatest {
                        if (it > 0 && it != emulationViewModel.totalShaders.value) {
                            binding.loadingProgressIndicator.isIndeterminate = false

                            if (it < binding.loadingProgressIndicator.max) {
                                binding.loadingProgressIndicator.progress = it
                            }
                        }

                        if (it == emulationViewModel.totalShaders.value) {
                            binding.loadingText.setText(R.string.loading)
                            binding.loadingProgressIndicator.isIndeterminate = true
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    emulationViewModel.totalShaders.collectLatest {
                        binding.loadingProgressIndicator.max = it
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    emulationViewModel.shaderMessage.collectLatest {
                        if (it.isNotEmpty()) {
                            binding.loadingText.text = it
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    driverViewModel.isInteractionAllowed.collect {
                        if (it) {
                            startEmulation()
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    emulationViewModel.emulationStarted.collectLatest {
                        if (it) {
                            binding.drawerLayout.setDrawerLockMode(IntSetting.LOCK_DRAWER.getInt())
                            ViewUtils.showView(binding.surfaceInputOverlay)
                            ViewUtils.hideView(binding.loadingIndicator)

                            emulationState.updateSurface()

                            // Setup overlay
                            updateShowFpsOverlay()
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    emulationViewModel.isEmulationStopping.collectLatest {
                        if (it) {
                            binding.loadingText.setText(R.string.shutting_down)
                            ViewUtils.showView(binding.loadingIndicator)
                            ViewUtils.hideView(binding.inputContainer)
                            ViewUtils.hideView(binding.showFpsText)
                        }
                    }
                }
            }
        }
    }

    private fun startEmulation() {
        if (!NativeLibrary.isRunning() && !NativeLibrary.isPaused()) {
            if (!DirectoryInitialization.areDirectoriesReady) {
                DirectoryInitialization.start()
            }

            updateScreenLayout()

            emulationState.run(emulationActivity!!.isActivityRecreated)
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        if (_binding == null) {
            return
        }

        updateScreenLayout()
        val showInputOverlay = BooleanSetting.SHOW_INPUT_OVERLAY.getBoolean()
        if (emulationActivity?.isInPictureInPictureMode == true) {
            if (binding.drawerLayout.isOpen) {
                binding.drawerLayout.close()
            }
            if (showInputOverlay) {
                binding.surfaceInputOverlay.visibility = View.INVISIBLE
            }
        } else {
            if (showInputOverlay && emulationViewModel.emulationStarted.value) {
                binding.surfaceInputOverlay.visibility = View.VISIBLE
            } else {
                binding.surfaceInputOverlay.visibility = View.INVISIBLE
            }
            if (!isInFoldableLayout) {
                if (newConfig.orientation == Configuration.ORIENTATION_PORTRAIT) {
                    binding.surfaceInputOverlay.layout = OverlayLayout.Portrait
                } else {
                    binding.surfaceInputOverlay.layout = OverlayLayout.Landscape
                }
            }
        }
    }

    override fun onPause() {
        if (emulationState.isRunning && emulationActivity?.isInPictureInPictureMode != true) {
            emulationState.pause()
        }
        super.onPause()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onDetach() {
        NativeLibrary.clearEmulationActivity()
        super.onDetach()
    }

    private fun resetInputOverlay() {
        IntSetting.OVERLAY_SCALE.reset()
        IntSetting.OVERLAY_OPACITY.reset()
        binding.surfaceInputOverlay.post {
            binding.surfaceInputOverlay.resetLayoutVisibilityAndPlacement()
        }
    }

    private fun updateShowFpsOverlay() {
        if (BooleanSetting.SHOW_PERFORMANCE_OVERLAY.getBoolean()) {
            val SYSTEM_FPS = 0
            val FPS = 1
            val FRAMETIME = 2
            val SPEED = 3
            perfStatsUpdater = {
                if (emulationViewModel.emulationStarted.value &&
                    !emulationViewModel.isEmulationStopping.value
                ) {
                    val perfStats = NativeLibrary.getPerfStats()
                    val cpuBackend = NativeLibrary.getCpuBackend()
                    val gpuDriver = NativeLibrary.getGpuDriver()
                    if (_binding != null) {
                        binding.showFpsText.text =
                            String.format("FPS: %.1f\n%s/%s", perfStats[FPS], cpuBackend, gpuDriver)
                    }
                    perfStatsUpdateHandler.postDelayed(perfStatsUpdater!!, 800)
                }
            }
            perfStatsUpdateHandler.post(perfStatsUpdater!!)
            binding.showFpsText.visibility = View.VISIBLE
        } else {
            if (perfStatsUpdater != null) {
                perfStatsUpdateHandler.removeCallbacks(perfStatsUpdater!!)
            }
            binding.showFpsText.visibility = View.GONE
        }
    }

    @SuppressLint("SourceLockedOrientationActivity")
    private fun updateOrientation() {
        emulationActivity?.let {
            val orientationSetting =
                EmulationOrientation.from(IntSetting.RENDERER_SCREEN_LAYOUT.getInt())
            it.requestedOrientation = when (orientationSetting) {
                EmulationOrientation.Unspecified -> ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
                EmulationOrientation.SensorLandscape ->
                    ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE

                EmulationOrientation.Landscape -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                EmulationOrientation.ReverseLandscape ->
                    ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE

                EmulationOrientation.SensorPortrait ->
                    ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT

                EmulationOrientation.Portrait -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                EmulationOrientation.ReversePortrait ->
                    ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
            }
        }
    }

    private fun updateScreenLayout() {
        binding.surfaceEmulation.setAspectRatio(null)
        emulationActivity?.buildPictureInPictureParams()
        updateOrientation()
    }

    private fun updateFoldableLayout(
        emulationActivity: EmulationActivity,
        newLayoutInfo: WindowLayoutInfo
    ) {
        val isFolding =
            (newLayoutInfo.displayFeatures.find { it is FoldingFeature } as? FoldingFeature)?.let {
                if (it.isSeparating) {
                    emulationActivity.requestedOrientation =
                        ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
                    if (it.orientation == FoldingFeature.Orientation.HORIZONTAL) {
                        // Restrict emulation and overlays to the top of the screen
                        binding.emulationContainer.layoutParams.height = it.bounds.top
                        // Restrict input and menu drawer to the bottom of the screen
                        binding.inputContainer.layoutParams.height = it.bounds.bottom
                        binding.inGameMenu.layoutParams.height = it.bounds.bottom

                        isInFoldableLayout = true
                        binding.surfaceInputOverlay.layout = OverlayLayout.Foldable
                    }
                }
                it.isSeparating
            } ?: false
        if (!isFolding) {
            binding.emulationContainer.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.inputContainer.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.inGameMenu.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            isInFoldableLayout = false
            updateOrientation()
            onConfigurationChanged(resources.configuration)
        }
        binding.emulationContainer.requestLayout()
        binding.inputContainer.requestLayout()
        binding.inGameMenu.requestLayout()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        // We purposely don't do anything here.
        // All work is done in surfaceChanged, which we are guaranteed to get even for surface creation.
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.debug("[EmulationFragment] Surface changed. Resolution: " + width + "x" + height)
        emulationState.newSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        emulationState.clearSurface()
    }

    private fun showOverlayOptions() {
        val anchor = binding.inGameMenu.findViewById<View>(R.id.menu_overlay_controls)
        val popup = PopupMenu(requireContext(), anchor)

        popup.menuInflater.inflate(R.menu.menu_overlay_options, popup.menu)

        popup.menu.apply {
            findItem(R.id.menu_toggle_fps).isChecked =
                BooleanSetting.SHOW_PERFORMANCE_OVERLAY.getBoolean()
            findItem(R.id.menu_rel_stick_center).isChecked =
                BooleanSetting.JOYSTICK_REL_CENTER.getBoolean()
            findItem(R.id.menu_dpad_slide).isChecked = BooleanSetting.DPAD_SLIDE.getBoolean()
            findItem(R.id.menu_show_overlay).isChecked =
                BooleanSetting.SHOW_INPUT_OVERLAY.getBoolean()
            findItem(R.id.menu_haptics).isChecked = BooleanSetting.HAPTIC_FEEDBACK.getBoolean()
            findItem(R.id.menu_touchscreen).isChecked = BooleanSetting.TOUCHSCREEN.getBoolean()
        }

        popup.setOnDismissListener { NativeConfig.saveGlobalConfig() }
        popup.setOnMenuItemClickListener {
            when (it.itemId) {
                R.id.menu_toggle_fps -> {
                    it.isChecked = !it.isChecked
                    BooleanSetting.SHOW_PERFORMANCE_OVERLAY.setBoolean(it.isChecked)
                    updateShowFpsOverlay()
                    true
                }

                R.id.menu_edit_overlay -> {
                    binding.drawerLayout.close()
                    binding.surfaceInputOverlay.requestFocus()
                    startConfiguringControls()
                    true
                }

                R.id.menu_adjust_overlay -> {
                    adjustOverlay()
                    true
                }

                R.id.menu_toggle_controls -> {
                    val overlayControlData = NativeConfig.getOverlayControlData()
                    val optionsArray = BooleanArray(overlayControlData.size)
                    overlayControlData.forEachIndexed { i, _ ->
                        optionsArray[i] = overlayControlData.firstOrNull { data ->
                            OverlayControl.entries[i].id == data.id
                        }?.enabled == true
                    }

                    val dialog = MaterialAlertDialogBuilder(requireContext())
                        .setTitle(R.string.emulation_toggle_controls)
                        .setMultiChoiceItems(
                            R.array.gamepadButtons,
                            optionsArray
                        ) { _, indexSelected, isChecked ->
                            overlayControlData.firstOrNull { data ->
                                OverlayControl.entries[indexSelected].id == data.id
                            }?.enabled = isChecked
                        }
                        .setPositiveButton(android.R.string.ok) { _, _ ->
                            NativeConfig.setOverlayControlData(overlayControlData)
                            NativeConfig.saveGlobalConfig()
                            binding.surfaceInputOverlay.refreshControls()
                        }
                        .setNegativeButton(android.R.string.cancel, null)
                        .setNeutralButton(R.string.emulation_toggle_all) { _, _ -> }
                        .show()

                    // Override normal behaviour so the dialog doesn't close
                    dialog.getButton(AlertDialog.BUTTON_NEUTRAL)
                        .setOnClickListener {
                            val isChecked = !optionsArray[0]
                            overlayControlData.forEachIndexed { i, _ ->
                                optionsArray[i] = isChecked
                                dialog.listView.setItemChecked(i, isChecked)
                                overlayControlData[i].enabled = isChecked
                            }
                        }
                    true
                }

                R.id.menu_show_overlay -> {
                    it.isChecked = !it.isChecked
                    BooleanSetting.SHOW_INPUT_OVERLAY.setBoolean(it.isChecked)
                    binding.surfaceInputOverlay.refreshControls()
                    true
                }

                R.id.menu_rel_stick_center -> {
                    it.isChecked = !it.isChecked
                    BooleanSetting.JOYSTICK_REL_CENTER.setBoolean(it.isChecked)
                    true
                }

                R.id.menu_dpad_slide -> {
                    it.isChecked = !it.isChecked
                    BooleanSetting.DPAD_SLIDE.setBoolean(it.isChecked)
                    true
                }

                R.id.menu_haptics -> {
                    it.isChecked = !it.isChecked
                    BooleanSetting.HAPTIC_FEEDBACK.setBoolean(it.isChecked)
                    true
                }

                R.id.menu_touchscreen -> {
                    it.isChecked = !it.isChecked
                    BooleanSetting.TOUCHSCREEN.setBoolean(it.isChecked)
                    true
                }

                R.id.menu_reset_overlay -> {
                    binding.drawerLayout.close()
                    resetInputOverlay()
                    true
                }

                else -> true
            }
        }

        popup.show()
    }

    @SuppressLint("SourceLockedOrientationActivity")
    private fun startConfiguringControls() {
        // Lock the current orientation to prevent editing inconsistencies
        if (IntSetting.RENDERER_SCREEN_LAYOUT.getInt() == EmulationOrientation.Unspecified.int) {
            emulationActivity?.let {
                it.requestedOrientation =
                    if (resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT) {
                        ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
                    } else {
                        ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
                    }
            }
        }
        binding.doneControlConfig.visibility = View.VISIBLE
        binding.surfaceInputOverlay.setIsInEditMode(true)
    }

    private fun stopConfiguringControls() {
        binding.doneControlConfig.visibility = View.GONE
        binding.surfaceInputOverlay.setIsInEditMode(false)
        // Unlock the orientation if it was locked for editing
        if (IntSetting.RENDERER_SCREEN_LAYOUT.getInt() == EmulationOrientation.Unspecified.int) {
            emulationActivity?.let {
                it.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            }
        }
        NativeConfig.saveGlobalConfig()
    }

    @SuppressLint("SetTextI18n")
    private fun adjustOverlay() {
        val adjustBinding = DialogOverlayAdjustBinding.inflate(layoutInflater)
        adjustBinding.apply {
            inputScaleSlider.apply {
                valueTo = 150F
                value = IntSetting.OVERLAY_SCALE.getInt().toFloat()
                addOnChangeListener(
                    Slider.OnChangeListener { _, value, _ ->
                        inputScaleValue.text = "${value.toInt()}%"
                        setControlScale(value.toInt())
                    }
                )
            }
            inputOpacitySlider.apply {
                valueTo = 100F
                value = IntSetting.OVERLAY_OPACITY.getInt().toFloat()
                addOnChangeListener(
                    Slider.OnChangeListener { _, value, _ ->
                        inputOpacityValue.text = "${value.toInt()}%"
                        setControlOpacity(value.toInt())
                    }
                )
            }
            inputScaleValue.text = "${inputScaleSlider.value.toInt()}%"
            inputOpacityValue.text = "${inputOpacitySlider.value.toInt()}%"
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.emulation_control_adjust)
            .setView(adjustBinding.root)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                NativeConfig.saveGlobalConfig()
            }
            .setNeutralButton(R.string.slider_default) { _: DialogInterface?, _: Int ->
                setControlScale(50)
                setControlOpacity(100)
            }
            .show()
    }

    private fun setControlScale(scale: Int) {
        IntSetting.OVERLAY_SCALE.setInt(scale)
        binding.surfaceInputOverlay.refreshControls()
    }

    private fun setControlOpacity(opacity: Int) {
        IntSetting.OVERLAY_OPACITY.setInt(opacity)
        binding.surfaceInputOverlay.refreshControls()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.inGameMenu
        ) { v: View, windowInsets: WindowInsetsCompat ->
            val cutInsets: Insets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            var left = 0
            var right = 0
            if (ViewCompat.getLayoutDirection(v) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                left = cutInsets.left
            } else {
                right = cutInsets.right
            }

            v.setPadding(left, cutInsets.top, right, 0)
            windowInsets
        }
    }

    private class EmulationState(
        private val gamePath: String,
        private val emulationCanStart: () -> Boolean
    ) {
        private var state: State
        private var surface: Surface? = null

        init {
            // Starting state is stopped.
            state = State.STOPPED
        }

        @get:Synchronized
        val isStopped: Boolean
            get() = state == State.STOPPED

        // Getters for the current state
        @get:Synchronized
        val isPaused: Boolean
            get() = state == State.PAUSED

        @get:Synchronized
        val isRunning: Boolean
            get() = state == State.RUNNING

        @Synchronized
        fun stop() {
            if (state != State.STOPPED) {
                Log.debug("[EmulationFragment] Stopping emulation.")
                NativeLibrary.stopEmulation()
                state = State.STOPPED
            } else {
                Log.warning("[EmulationFragment] Stop called while already stopped.")
            }
        }

        // State changing methods
        @Synchronized
        fun pause() {
            if (state != State.PAUSED) {
                Log.debug("[EmulationFragment] Pausing emulation.")

                NativeLibrary.pauseEmulation()

                state = State.PAUSED
            } else {
                Log.warning("[EmulationFragment] Pause called while already paused.")
            }
        }

        @Synchronized
        fun run(isActivityRecreated: Boolean) {
            if (isActivityRecreated) {
                if (NativeLibrary.isRunning()) {
                    state = State.PAUSED
                }
            } else {
                Log.debug("[EmulationFragment] activity resumed or fresh start")
            }

            // If the surface is set, run now. Otherwise, wait for it to get set.
            if (surface != null) {
                runWithValidSurface()
            }
        }

        // Surface callbacks
        @Synchronized
        fun newSurface(surface: Surface?) {
            this.surface = surface
            if (this.surface != null) {
                runWithValidSurface()
            }
        }

        @Synchronized
        fun updateSurface() {
            if (surface != null) {
                NativeLibrary.surfaceChanged(surface)
            }
        }

        @Synchronized
        fun clearSurface() {
            if (surface == null) {
                Log.warning("[EmulationFragment] clearSurface called, but surface already null.")
            } else {
                surface = null
                Log.debug("[EmulationFragment] Surface destroyed.")
                when (state) {
                    State.RUNNING -> {
                        state = State.PAUSED
                    }

                    State.PAUSED -> Log.warning(
                        "[EmulationFragment] Surface cleared while emulation paused."
                    )

                    else -> Log.warning(
                        "[EmulationFragment] Surface cleared while emulation stopped."
                    )
                }
            }
        }

        private fun runWithValidSurface() {
            NativeLibrary.surfaceChanged(surface)
            if (!emulationCanStart.invoke()) {
                return
            }

            when (state) {
                State.STOPPED -> {
                    val emulationThread = Thread({
                        Log.debug("[EmulationFragment] Starting emulation thread.")
                        NativeLibrary.run(gamePath)
                    }, "NativeEmulation")
                    emulationThread.start()
                }

                State.PAUSED -> {
                    Log.debug("[EmulationFragment] Resuming emulation.")
                    NativeLibrary.unpauseEmulation()
                }

                else -> Log.debug("[EmulationFragment] Bug, run called while already running.")
            }
            state = State.RUNNING
        }

        private enum class State {
            STOPPED, RUNNING, PAUSED
        }
    }

    companion object {
        private val perfStatsUpdateHandler = Handler(Looper.myLooper()!!)
    }
}
