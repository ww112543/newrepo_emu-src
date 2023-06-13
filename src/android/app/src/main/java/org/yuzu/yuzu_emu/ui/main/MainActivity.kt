// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.main

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.view.WindowManager
import android.view.animation.PathInterpolator
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import androidx.preference.PreferenceManager
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.navigation.NavigationBarView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.ActivityMainBinding
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.SettingsViewModel
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.fragments.IndeterminateProgressDialogFragment
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.utils.*
import java.io.File
import java.io.FilenameFilter
import java.io.IOException

class MainActivity : AppCompatActivity(), ThemeProvider {
    private lateinit var binding: ActivityMainBinding

    private val homeViewModel: HomeViewModel by viewModels()
    private val gamesViewModel: GamesViewModel by viewModels()
    private val settingsViewModel: SettingsViewModel by viewModels()

    override var themeId: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()
        splashScreen.setKeepOnScreenCondition { !DirectoryInitialization.areDirectoriesReady }

        settingsViewModel.settings.loadSettings()

        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)

        window.statusBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)
        window.navigationBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)

        binding.statusBarShade.setBackgroundColor(
            ThemeHelper.getColorWithOpacity(
                MaterialColors.getColor(
                    binding.root,
                    com.google.android.material.R.attr.colorSurface
                ),
                ThemeHelper.SYSTEM_BAR_ALPHA
            )
        )
        if (InsetsHelper.getSystemGestureType(applicationContext) != InsetsHelper.GESTURE_NAVIGATION) {
            binding.navigationBarShade.setBackgroundColor(
                ThemeHelper.getColorWithOpacity(
                    MaterialColors.getColor(
                        binding.root,
                        com.google.android.material.R.attr.colorSurface
                    ),
                    ThemeHelper.SYSTEM_BAR_ALPHA
                )
            )
        }

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        setUpNavigation(navHostFragment.navController)
        (binding.navigationView as NavigationBarView).setOnItemReselectedListener {
            when (it.itemId) {
                R.id.gamesFragment -> gamesViewModel.setShouldScrollToTop(true)
                R.id.searchFragment -> gamesViewModel.setSearchFocused(true)
                R.id.homeSettingsFragment -> SettingsActivity.launch(
                    this,
                    SettingsFile.FILE_NAME_CONFIG,
                    ""
                )
            }
        }

        // Prevents navigation from being drawn for a short time on recreation if set to hidden
        if (!homeViewModel.navigationVisible.value?.first!!) {
            binding.navigationView.visibility = View.INVISIBLE
            binding.statusBarShade.visibility = View.INVISIBLE
        }

        homeViewModel.navigationVisible.observe(this) {
            showNavigation(it.first, it.second)
        }
        homeViewModel.statusBarShadeVisible.observe(this) { visible ->
            showStatusBarShade(visible)
        }

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.stopForegroundService(this)

        setInsets()
    }

    fun finishSetup(navController: NavController) {
        navController.navigate(R.id.action_firstTimeSetupFragment_to_gamesFragment)
        (binding.navigationView as NavigationBarView).setupWithNavController(navController)
        showNavigation(visible = true, animated = true)
    }

    private fun setUpNavigation(navController: NavController) {
        val firstTimeSetup = PreferenceManager.getDefaultSharedPreferences(applicationContext)
            .getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)

        if (firstTimeSetup && !homeViewModel.navigatedToSetup) {
            navController.navigate(R.id.firstTimeSetupFragment)
            homeViewModel.navigatedToSetup = true
        } else {
            (binding.navigationView as NavigationBarView).setupWithNavController(navController)
        }
    }

    private fun showNavigation(visible: Boolean, animated: Boolean) {
        if (!animated) {
            if (visible) {
                binding.navigationView.visibility = View.VISIBLE
            } else {
                binding.navigationView.visibility = View.INVISIBLE
            }
            return
        }

        val smallLayout = resources.getBoolean(R.bool.small_layout)
        binding.navigationView.animate().apply {
            if (visible) {
                binding.navigationView.visibility = View.VISIBLE
                duration = 300
                interpolator = PathInterpolator(0.05f, 0.7f, 0.1f, 1f)

                if (smallLayout) {
                    binding.navigationView.translationY =
                        binding.navigationView.height.toFloat() * 2
                    translationY(0f)
                } else {
                    if (ViewCompat.getLayoutDirection(binding.navigationView) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                        binding.navigationView.translationX =
                            binding.navigationView.width.toFloat() * -2
                        translationX(0f)
                    } else {
                        binding.navigationView.translationX =
                            binding.navigationView.width.toFloat() * 2
                        translationX(0f)
                    }
                }
            } else {
                duration = 300
                interpolator = PathInterpolator(0.3f, 0f, 0.8f, 0.15f)

                if (smallLayout) {
                    translationY(binding.navigationView.height.toFloat() * 2)
                } else {
                    if (ViewCompat.getLayoutDirection(binding.navigationView) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                        translationX(binding.navigationView.width.toFloat() * -2)
                    } else {
                        translationX(binding.navigationView.width.toFloat() * 2)
                    }
                }
            }
        }.withEndAction {
            if (!visible) {
                binding.navigationView.visibility = View.INVISIBLE
            }
        }.start()
    }

    private fun showStatusBarShade(visible: Boolean) {
        binding.statusBarShade.animate().apply {
            if (visible) {
                binding.statusBarShade.visibility = View.VISIBLE
                binding.statusBarShade.translationY = binding.statusBarShade.height.toFloat() * -2
                duration = 300
                translationY(0f)
                interpolator = PathInterpolator(0.05f, 0.7f, 0.1f, 1f)
            } else {
                duration = 300
                translationY(binding.navigationView.height.toFloat() * -2)
                interpolator = PathInterpolator(0.3f, 0f, 0.8f, 0.15f)
            }
        }.withEndAction {
            if (!visible) {
                binding.statusBarShade.visibility = View.INVISIBLE
            }
        }.start()
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)
        super.onResume()
    }

    override fun onDestroy() {
        EmulationActivity.stopForegroundService(this)
        super.onDestroy()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val mlpStatusShade = binding.statusBarShade.layoutParams as MarginLayoutParams
            mlpStatusShade.height = insets.top
            binding.statusBarShade.layoutParams = mlpStatusShade

            // The only situation where we care to have a nav bar shade is when it's at the bottom
            // of the screen where scrolling list elements can go behind it.
            val mlpNavShade = binding.navigationBarShade.layoutParams as MarginLayoutParams
            mlpNavShade.height = insets.bottom
            binding.navigationBarShade.layoutParams = mlpNavShade

            windowInsets
        }

    override fun setTheme(resId: Int) {
        super.setTheme(resId)
        themeId = resId
    }

    val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result == null)
                return@registerForActivityResult

            contentResolver.takePersistableUriPermission(
                result,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )

            // When a new directory is picked, we currently will reset the existing games
            // database. This effectively means that only one game directory is supported.
            PreferenceManager.getDefaultSharedPreferences(applicationContext).edit()
                .putString(GameHelper.KEY_GAME_PATH, result.toString())
                .apply()

            Toast.makeText(
                applicationContext,
                R.string.games_dir_selected,
                Toast.LENGTH_LONG
            ).show()

            gamesViewModel.reloadGames(true)
        }

    val getProdKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            if (!FileUtil.hasExtension(result, "keys")) {
                MessageDialogFragment.newInstance(
                    R.string.reading_keys_failure,
                    R.string.install_prod_keys_failure_extension_description
                ).show(supportFragmentManager, MessageDialogFragment.TAG)
                return@registerForActivityResult
            }

            contentResolver.takePersistableUriPermission(
                result,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )

            val dstPath = DirectoryInitialization.userDirectory + "/keys/"
            if (FileUtil.copyUriToInternalStorage(
                    applicationContext,
                    result,
                    dstPath,
                    "prod.keys"
                )
            ) {
                if (NativeLibrary.reloadKeys()) {
                    Toast.makeText(
                        applicationContext,
                        R.string.install_keys_success,
                        Toast.LENGTH_SHORT
                    ).show()
                    gamesViewModel.reloadGames(true)
                } else {
                    MessageDialogFragment.newInstance(
                        R.string.invalid_keys_error,
                        R.string.install_keys_failure_description,
                        R.string.dumping_keys_quickstart_link
                    ).show(supportFragmentManager, MessageDialogFragment.TAG)
                }
            }
        }

    val getFirmware =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val inputZip = contentResolver.openInputStream(result)
            if (inputZip == null) {
                Toast.makeText(
                    applicationContext,
                    getString(R.string.fatal_error),
                    Toast.LENGTH_LONG
                ).show()
                return@registerForActivityResult
            }

            val filterNCA = FilenameFilter { _, dirName -> dirName.endsWith(".nca") }

            val firmwarePath =
                File(DirectoryInitialization.userDirectory + "/nand/system/Contents/registered/")
            val cacheFirmwareDir = File("${cacheDir.path}/registered/")

            val task: () -> Any = {
                var messageToShow: Any
                try {
                    FileUtil.unzip(inputZip, cacheFirmwareDir)
                    val unfilteredNumOfFiles = cacheFirmwareDir.list()?.size ?: -1
                    val filteredNumOfFiles = cacheFirmwareDir.list(filterNCA)?.size ?: -2
                    messageToShow = if (unfilteredNumOfFiles != filteredNumOfFiles) {
                        MessageDialogFragment.newInstance(
                            R.string.firmware_installed_failure,
                            R.string.firmware_installed_failure_description
                        )
                    } else {
                        firmwarePath.deleteRecursively()
                        cacheFirmwareDir.copyRecursively(firmwarePath, true)
                        getString(R.string.save_file_imported_success)
                    }
                } catch (e: Exception) {
                    messageToShow = getString(R.string.fatal_error)
                } finally {
                    cacheFirmwareDir.deleteRecursively()
                }
                messageToShow
            }

            IndeterminateProgressDialogFragment.newInstance(
                this,
                R.string.firmware_installing,
                task
            ).show(supportFragmentManager, IndeterminateProgressDialogFragment.TAG)
        }

    val getAmiiboKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            if (!FileUtil.hasExtension(result, "bin")) {
                MessageDialogFragment.newInstance(
                    R.string.reading_keys_failure,
                    R.string.install_amiibo_keys_failure_extension_description
                ).show(supportFragmentManager, MessageDialogFragment.TAG)
                return@registerForActivityResult
            }

            contentResolver.takePersistableUriPermission(
                result,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )

            val dstPath = DirectoryInitialization.userDirectory + "/keys/"
            if (FileUtil.copyUriToInternalStorage(
                    applicationContext,
                    result,
                    dstPath,
                    "key_retail.bin"
                )
            ) {
                if (NativeLibrary.reloadKeys()) {
                    Toast.makeText(
                        applicationContext,
                        R.string.install_keys_success,
                        Toast.LENGTH_SHORT
                    ).show()
                } else {
                    MessageDialogFragment.newInstance(
                        R.string.invalid_keys_error,
                        R.string.install_keys_failure_description,
                        R.string.dumping_keys_quickstart_link
                    ).show(supportFragmentManager, MessageDialogFragment.TAG)
                }
            }
        }

    val getDriver =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val progressBinding = DialogProgressBarBinding.inflate(layoutInflater)
            progressBinding.progressBar.isIndeterminate = true
            val installationDialog = MaterialAlertDialogBuilder(this)
                .setTitle(R.string.installing_driver)
                .setView(progressBinding.root)
                .show()

            lifecycleScope.launch {
                withContext(Dispatchers.IO) {
                    // Ignore file exceptions when a user selects an invalid zip
                    try {
                        GpuDriverHelper.installCustomDriver(applicationContext, result)
                    } catch (_: IOException) {
                    }

                    withContext(Dispatchers.Main) {
                        installationDialog.dismiss()

                        val driverName = GpuDriverHelper.customDriverName
                        if (driverName != null) {
                            Toast.makeText(
                                applicationContext,
                                getString(
                                    R.string.select_gpu_driver_install_success,
                                    driverName
                                ),
                                Toast.LENGTH_SHORT
                            ).show()
                        } else {
                            Toast.makeText(
                                applicationContext,
                                R.string.select_gpu_driver_error,
                                Toast.LENGTH_LONG
                            ).show()
                        }
                    }
                }
            }
        }

    val installGameUpdate =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) {
            if (it == null)
                return@registerForActivityResult

            IndeterminateProgressDialogFragment.newInstance(
                this@MainActivity,
                R.string.install_game_content
            ) {
                val result = NativeLibrary.installFileToNand(it.toString())
                lifecycleScope.launch {
                    withContext(Dispatchers.Main) {
                        when (result) {
                            NativeLibrary.InstallFileToNandResult.Success -> {
                                Toast.makeText(
                                    applicationContext,
                                    R.string.install_game_content_success,
                                    Toast.LENGTH_SHORT
                                ).show()
                            }

                            NativeLibrary.InstallFileToNandResult.SuccessFileOverwritten -> {
                                Toast.makeText(
                                    applicationContext,
                                    R.string.install_game_content_success_overwrite,
                                    Toast.LENGTH_SHORT
                                ).show()
                            }

                            NativeLibrary.InstallFileToNandResult.ErrorBaseGame -> {
                                MessageDialogFragment.newInstance(
                                    R.string.install_game_content_failure,
                                    R.string.install_game_content_failure_base
                                ).show(supportFragmentManager, MessageDialogFragment.TAG)
                            }

                            NativeLibrary.InstallFileToNandResult.ErrorFilenameExtension -> {
                                MessageDialogFragment.newInstance(
                                    R.string.install_game_content_failure,
                                    R.string.install_game_content_failure_file_extension,
                                    R.string.install_game_content_help_link
                                ).show(supportFragmentManager, MessageDialogFragment.TAG)
                            }

                            else -> {
                                MessageDialogFragment.newInstance(
                                    R.string.install_game_content_failure,
                                    R.string.install_game_content_failure_description,
                                    R.string.install_game_content_help_link
                                ).show(supportFragmentManager, MessageDialogFragment.TAG)
                            }
                        }
                    }
                }
                return@newInstance result
            }.show(supportFragmentManager, IndeterminateProgressDialogFragment.TAG)
        }
}
