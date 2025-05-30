/*
 * Copyright (c) 2012, 2025, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package com.sun.javafx.font;

import java.io.File;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.nio.file.Files;
/*
 * Utility class to write sfnt-based font files.
 *
 * To reduce the number of IO operation this class buffers the font header
 * and directory when the API writeHeader() and writeDirectoryEntry() are used.
 */
class FontFileWriter implements FontConstants {
    byte[] header;              // buffer for the header and directory
    int pos;                    // current position for the tables
    int headerPos;              // current buffer position in the header
    File file;
    RandomAccessFile raFile;

    public FontFileWriter() {
    }

    protected void setLength(int size) throws IOException {
        if (raFile == null) {
            throw new IOException("File not open");
        }
        raFile.setLength(size);
    }

    public void seek(int pos) throws IOException {
        if (raFile == null) {
            throw new IOException("File not open");
        }
        if (pos != this.pos) {
            raFile.seek(pos);
            this.pos = pos;
        }
    }

    public File getFile() {
        return file;
    }

    public File openFile() throws IOException {
        pos = 0;
        try {
            file = Files.createTempFile("+JXF", ".tmp").toFile();
        } catch (IOException e) {
            // don't reveal temporary directory location
            throw new IOException("Unable to create temporary file");
        }
        raFile = new RandomAccessFile(file, "rw");
        if (PrismFontFactory.debugFonts) {
            System.err.println("Temp file created: " + file.getPath());
        }
        return file;
    }

    public void closeFile() throws IOException {
        if (header != null) {
            raFile.seek(0);
            raFile.write(header);
            header = null;
        }
        if (raFile != null) {
            raFile.close();
            raFile = null;
        }
    }

    public void deleteFile() {
        if (file != null) {
            try {
                closeFile();
            } catch (Exception e) {
            }
            try {
                file.delete();
                if (PrismFontFactory.debugFonts) {
                    System.err.println("Temp file delete: " + file.getPath());
                }
            } catch (Exception e) {
            }
            file = null;
            raFile = null;
        }
    }

    private void setHeaderPos(int pos) {
        headerPos = pos;
    }

    /*
     * Write a snft header for the specified format and number of tables.
     */
    public void writeHeader(int format, short numTables) throws IOException {
        int size = TTCHEADERSIZE + (DIRECTORYENTRYSIZE * numTables);
        header = new byte[size];

        /* Spec:
        * searchRange = (maximum power of 2 <= numTables) * 16
        * entrySelector = log2(maximum power of 2 <= numTables)
        * rangeShift = numTables*16-searchRange
        */
        short maxPower2 = numTables;
        maxPower2 |= (maxPower2 >> 1);
        maxPower2 |= (maxPower2 >> 2);
        maxPower2 |= (maxPower2 >> 4);
        maxPower2 |= (maxPower2 >> 8);
        /* at this point maxPower2+1 is the minimum power of 2 > numTables
          maxPower2 & ~(maxPower2>>1) is the maximum power of 2 <= numTables */
        maxPower2 &= ~(maxPower2 >> 1);
        short searchRange = (short)(maxPower2 * 16);
        short entrySelector = 0;
        while (maxPower2 > 1) {
            entrySelector++;
            maxPower2 >>= 1;
        }
        short rangeShift = (short)(numTables * 16 - searchRange);

        setHeaderPos(0);
        writeInt(format);
        writeShort(numTables);
        writeShort(searchRange);
        writeShort(entrySelector);
        writeShort(rangeShift);
    }

    public void writeDirectoryEntry(int index, int tag, int checksum, int offset, int length) {
        setHeaderPos(TTCHEADERSIZE + DIRECTORYENTRYSIZE * index);
        writeInt(tag);
        writeInt(checksum);
        writeInt(offset);
        writeInt(length);
    }

    private void writeInt(int value) {
        header[headerPos++] = (byte)((value & 0xFF000000) >> 24);
        header[headerPos++] = (byte)((value & 0x00FF0000) >> 16);
        header[headerPos++] = (byte)((value & 0x0000FF00) >> 8);
        header[headerPos++] = (byte) (value & 0x000000FF);
    }

    private void writeShort(short value) {
        header[headerPos++] = (byte)((value & 0xFF00) >> 8);
        header[headerPos++] = (byte)(value & 0xFF);
    }

    public void writeBytes(byte[] buffer) throws IOException {
        writeBytes(buffer, 0, buffer.length);
    }

    public void writeBytes(byte[] buffer, int startPos, int length)
            throws IOException
    {
        raFile.write(buffer, startPos, length);
        pos += length;
    }
}
