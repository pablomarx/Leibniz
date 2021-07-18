mergeInto(LibraryManager.library, {
	newton_display_open: function(width, height) {
		lcd_set_size(width, height);
	},
	newton_display_set_framebuffer: function(pixels_ptr, width, height) {
		var length = width * height;
		var pixels = HEAPU8.subarray(pixels_ptr, pixels_ptr + length);
		lcd_set_pixels(pixels, width, height);
	},
});