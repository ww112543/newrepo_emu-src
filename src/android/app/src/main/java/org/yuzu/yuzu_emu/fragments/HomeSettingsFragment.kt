// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.documentfile.provider.DocumentFile
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.BuildConfig
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.HomeSettingAdapter
import org.yuzu.yuzu_emu.databinding.FragmentHomeSettingsBinding
import org.yuzu.yuzu_emu.features.DocumentProvider
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.HomeSetting
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.ui.main.MainActivity
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GpuDriverHelper

class HomeSettingsFragment : Fragment() {
    private var _binding: FragmentHomeSettingsBinding? = null
    private val binding get() = _binding!!

    private lateinit var mainActivity: MainActivity

    private val homeViewModel: HomeViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentHomeSettingsBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        mainActivity = requireActivity() as MainActivity

        val optionsList: MutableList<HomeSetting> = mutableListOf(
            HomeSetting(
                R.string.advanced_settings,
                R.string.settings_description,
                R.drawable.ic_settings
            ) { SettingsActivity.launch(requireContext(), SettingsFile.FILE_NAME_CONFIG, "") },
            HomeSetting(
                R.string.open_user_folder,
                R.string.open_user_folder_description,
                R.drawable.ic_folder_open
            ) { openFileManager() },
            HomeSetting(
                R.string.preferences_theme,
                R.string.theme_and_color_description,
                R.drawable.ic_palette
            ) { SettingsActivity.launch(requireContext(), Settings.SECTION_THEME, "") },
            HomeSetting(
                R.string.install_gpu_driver,
                R.string.install_gpu_driver_description,
                R.drawable.ic_exit
            ) { driverInstaller() },
            HomeSetting(
                R.string.install_amiibo_keys,
                R.string.install_amiibo_keys_description,
                R.drawable.ic_nfc
            ) { mainActivity.getAmiiboKey.launch(arrayOf("*/*")) },
            HomeSetting(
                R.string.install_game_content,
                R.string.install_game_content_description,
                R.drawable.ic_system_update_alt
            ) { mainActivity.installGameUpdate.launch(arrayOf("*/*")) },
            HomeSetting(
                R.string.select_games_folder,
                R.string.select_games_folder_description,
                R.drawable.ic_add
            ) {
                mainActivity.getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data)
            },
            HomeSetting(
                R.string.manage_save_data,
                R.string.import_export_saves_description,
                R.drawable.ic_save
            ) {
                ImportExportSavesFragment().show(
                    parentFragmentManager,
                    ImportExportSavesFragment.TAG
                )
            },
            HomeSetting(
                R.string.install_prod_keys,
                R.string.install_prod_keys_description,
                R.drawable.ic_unlock
            ) { mainActivity.getProdKey.launch(arrayOf("*/*")) },
            HomeSetting(
                R.string.install_firmware,
                R.string.install_firmware_description,
                R.drawable.ic_firmware
            ) { mainActivity.getFirmware.launch(arrayOf("application/zip")) },
            HomeSetting(
                R.string.share_log,
                R.string.share_log_description,
                R.drawable.ic_log
            ) { shareLog() },
            HomeSetting(
                R.string.about,
                R.string.about_description,
                R.drawable.ic_info_outline
            ) {
                exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
                parentFragmentManager.primaryNavigationFragment?.findNavController()
                    ?.navigate(R.id.action_homeSettingsFragment_to_aboutFragment)
            }
        )

        if (!BuildConfig.PREMIUM) {
            optionsList.add(
                0,
                HomeSetting(
                    R.string.get_early_access,
                    R.string.get_early_access_description,
                    R.drawable.ic_diamond
                ) {
                    exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
                    parentFragmentManager.primaryNavigationFragment?.findNavController()
                        ?.navigate(R.id.action_homeSettingsFragment_to_earlyAccessFragment)
                }
            )
        }

        binding.homeSettingsList.apply {
            layoutManager = LinearLayoutManager(requireContext())
            adapter = HomeSettingAdapter(requireActivity() as AppCompatActivity, optionsList)
        }

        setInsets()
    }

    override fun onStart() {
        super.onStart()
        exitTransition = null
        homeViewModel.setNavigationVisibility(visible = true, animated = true)
        homeViewModel.setStatusBarShadeVisibility(visible = true)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun openFileManager() {
        // First, try to open the user data folder directly
        try {
            startActivity(getFileManagerIntentOnDocumentProvider(Intent.ACTION_VIEW))
            return
        } catch (_: ActivityNotFoundException) {
        }

        try {
            startActivity(getFileManagerIntentOnDocumentProvider("android.provider.action.BROWSE"))
            return
        } catch (_: ActivityNotFoundException) {
        }

        // Just try to open the file manager, try the package name used on "normal" phones
        try {
            startActivity(getFileManagerIntent("com.google.android.documentsui"))
            showNoLinkNotification()
            return
        } catch (_: ActivityNotFoundException) {
        }

        try {
            // Next, try the AOSP package name
            startActivity(getFileManagerIntent("com.android.documentsui"))
            showNoLinkNotification()
            return
        } catch (_: ActivityNotFoundException) {
        }

        Toast.makeText(
            requireContext(),
            resources.getString(R.string.no_file_manager),
            Toast.LENGTH_LONG
        ).show()
    }

    private fun getFileManagerIntent(packageName: String): Intent {
        // Fragile, but some phones don't expose the system file manager in any better way
        val intent = Intent(Intent.ACTION_MAIN)
        intent.setClassName(packageName, "com.android.documentsui.files.FilesActivity")
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        return intent
    }

    private fun getFileManagerIntentOnDocumentProvider(action: String): Intent {
        val authority = "${requireContext().packageName}.user"
        val intent = Intent(action)
        intent.addCategory(Intent.CATEGORY_DEFAULT)
        intent.data = DocumentsContract.buildRootUri(authority, DocumentProvider.ROOT_ID)
        intent.addFlags(
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or
                Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
        return intent
    }

    private fun showNoLinkNotification() {
        val builder = NotificationCompat.Builder(
            requireContext(),
            getString(R.string.notice_notification_channel_id)
        )
            .setSmallIcon(R.drawable.ic_stat_notification_logo)
            .setContentTitle(getString(R.string.notification_no_directory_link))
            .setContentText(getString(R.string.notification_no_directory_link_description))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setAutoCancel(true)
        // TODO: Make the click action for this notification lead to a help article

        with(NotificationManagerCompat.from(requireContext())) {
            if (ActivityCompat.checkSelfPermission(
                    requireContext(),
                    Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                Toast.makeText(
                    requireContext(),
                    resources.getString(R.string.notification_permission_not_granted),
                    Toast.LENGTH_LONG
                ).show()
                return
            }
            notify(0, builder.build())
        }
    }

    private fun driverInstaller() {
        // Get the driver name for the dialog message.
        var driverName = GpuDriverHelper.customDriverName
        if (driverName == null) {
            driverName = getString(R.string.system_gpu_driver)
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.select_gpu_driver_title))
            .setMessage(driverName)
            .setNegativeButton(android.R.string.cancel, null)
            .setNeutralButton(R.string.select_gpu_driver_default) { _: DialogInterface?, _: Int ->
                GpuDriverHelper.installDefaultDriver(requireContext())
                Toast.makeText(
                    requireContext(),
                    R.string.select_gpu_driver_use_default,
                    Toast.LENGTH_SHORT
                ).show()
            }
            .setPositiveButton(R.string.select_gpu_driver_install) { _: DialogInterface?, _: Int ->
                mainActivity.getDriver.launch(arrayOf("application/zip"))
            }
            .show()
    }

    private fun shareLog() {
        val file = DocumentFile.fromSingleUri(
            mainActivity,
            DocumentsContract.buildDocumentUri(
                DocumentProvider.AUTHORITY,
                "${DocumentProvider.ROOT_ID}/log/yuzu_log.txt"
            )
        )!!
        if (file.exists()) {
            val intent = Intent(Intent.ACTION_SEND)
                .setDataAndType(file.uri, FileUtil.TEXT_PLAIN)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                .putExtra(Intent.EXTRA_STREAM, file.uri)
            startActivity(Intent.createChooser(intent, getText(R.string.share_log)))
        } else {
            Toast.makeText(
                requireContext(),
                getText(R.string.share_log_missing),
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val spacingNavigation = resources.getDimensionPixelSize(R.dimen.spacing_navigation)
            val spacingNavigationRail =
                resources.getDimensionPixelSize(R.dimen.spacing_navigation_rail)

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.scrollViewSettings.updatePadding(
                top = barInsets.top,
                bottom = barInsets.bottom
            )

            val mlpScrollSettings = binding.scrollViewSettings.layoutParams as MarginLayoutParams
            mlpScrollSettings.leftMargin = leftInsets
            mlpScrollSettings.rightMargin = rightInsets
            binding.scrollViewSettings.layoutParams = mlpScrollSettings

            binding.linearLayoutSettings.updatePadding(bottom = spacingNavigation)

            if (ViewCompat.getLayoutDirection(view) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                binding.linearLayoutSettings.updatePadding(left = spacingNavigationRail)
            } else {
                binding.linearLayoutSettings.updatePadding(right = spacingNavigationRail)
            }

            windowInsets
        }
}
