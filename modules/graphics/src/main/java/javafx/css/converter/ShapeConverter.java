/*
 * Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.
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

package javafx.css.converter;

import javafx.css.ParsedValue;
import javafx.css.StyleConverter;
import javafx.scene.shape.SVGPath;
import javafx.scene.shape.Shape;
import javafx.scene.text.Font;

import java.util.Map;

/**
 * Converts an SVG shape string into a Shape object.
 *
 * @since 9
 */
public class ShapeConverter extends StyleConverter<String, Shape> {
    private static final ShapeConverter INSTANCE = new ShapeConverter();

    public static StyleConverter<String, Shape> getInstance() { return INSTANCE; }

    @Override public Shape convert(ParsedValue<String, Shape> value, Font font) {

        Shape shape = super.getCachedValue(value);
        if (shape != null) return shape;

        String svg = value.getValue();
        if (svg == null || svg.isEmpty()) return null;
        SVGPath path = new SVGPath();
        path.setContent(svg);

        super.cacheValue(value, path);

        return path;
    }

    private static Map<ParsedValue<String, Shape>, Shape> cache;

    public static void clearCache() { if (cache != null) cache.clear(); }

}