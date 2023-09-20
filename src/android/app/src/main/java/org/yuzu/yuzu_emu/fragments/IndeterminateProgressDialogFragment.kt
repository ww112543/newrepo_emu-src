// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding
import org.yuzu.yuzu_emu.model.TaskViewModel

class IndeterminateProgressDialogFragment : DialogFragment() {
    private val taskViewModel: TaskViewModel by activityViewModels()

    private lateinit var binding: DialogProgressBarBinding

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val titleId = requireArguments().getInt(TITLE)
        val cancellable = requireArguments().getBoolean(CANCELLABLE)

        binding = DialogProgressBarBinding.inflate(layoutInflater)
        binding.progressBar.isIndeterminate = true
        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(titleId)
            .setView(binding.root)

        if (cancellable) {
            dialog.setNegativeButton(android.R.string.cancel) { _: DialogInterface, _: Int ->
                taskViewModel.setCancelled(true)
            }
        }

        val alertDialog = dialog.create()
        alertDialog.setCanceledOnTouchOutside(false)

        if (!taskViewModel.isRunning.value) {
            taskViewModel.runTask()
        }
        return alertDialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    taskViewModel.isComplete.collect {
                        if (it) {
                            dismiss()
                            when (val result = taskViewModel.result.value) {
                                is String -> Toast.makeText(
                                    requireContext(),
                                    result,
                                    Toast.LENGTH_LONG
                                ).show()

                                is MessageDialogFragment -> result.show(
                                    requireActivity().supportFragmentManager,
                                    MessageDialogFragment.TAG
                                )
                            }
                            taskViewModel.clear()
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    taskViewModel.cancelled.collect {
                        if (it) {
                            dialog?.setTitle(R.string.cancelling)
                        }
                    }
                }
            }
        }
    }

    companion object {
        const val TAG = "IndeterminateProgressDialogFragment"

        private const val TITLE = "Title"
        private const val CANCELLABLE = "Cancellable"

        fun newInstance(
            activity: AppCompatActivity,
            titleId: Int,
            cancellable: Boolean = false,
            task: () -> Any
        ): IndeterminateProgressDialogFragment {
            val dialog = IndeterminateProgressDialogFragment()
            val args = Bundle()
            ViewModelProvider(activity)[TaskViewModel::class.java].task = task
            args.putInt(TITLE, titleId)
            args.putBoolean(CANCELLABLE, cancellable)
            dialog.arguments = args
            return dialog
        }
    }
}
