/* -*- mode: c; style: linux -*- */

/* file-transfer-dialog.h
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by Rachel Hestilow <hestilow@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __FILE_TRANSFER_DIALOG_H__
#define __FILE_TRANSFER_DIALOG_H__

#include <ctk/ctk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_TRANSFER_DIALOG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, file_transfer_dialog_get_type (), FileTransferDialog)
#define FILE_TRANSFER_DIALOG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, file_transfer_dialog_get_type (), FileTransferDialogClass)
#define IS_FILE_TRANSFER_DIALOG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, file_transfer_dialog_get_type ())

typedef struct _FileTransferDialog FileTransferDialog;
typedef struct _FileTransferDialogClass FileTransferDialogClass;
typedef struct _FileTransferDialogPrivate FileTransferDialogPrivate;

typedef enum {
	FILE_TRANSFER_DIALOG_DEFAULT = 1 << 0,
	FILE_TRANSFER_DIALOG_OVERWRITE = 1 << 1
} FileTransferDialogOptions;

struct _FileTransferDialog
{
	CtkDialog dialog;

	FileTransferDialogPrivate *priv;
};

struct _FileTransferDialogClass
{
	CtkDialogClass parent_class;
};

GType	       file_transfer_dialog_get_type (void);
CtkWidget*     file_transfer_dialog_new (void);
CtkWidget*     file_transfer_dialog_new_with_parent (CtkWindow *parent);

void	       file_transfer_dialog_copy_async (FileTransferDialog *dlg,
						GList *source_files,
						GList *target_files,
						FileTransferDialogOptions options,
						int priority);


#ifdef __cplusplus
}
#endif

#endif /* __FILE_TRANSFER_DIALOG_H__ */
