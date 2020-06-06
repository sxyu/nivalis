var pickrConfig = {
    theme: 'monolith',
    comparison: false,
    lockOpacity: true,
    padding: 0,
    swatches: [
        '#FF0000', '#4169E1',
        '#008000', '#FFA500',
        '#800080', '#000000',
        'rgba(255, 116, 0, 1)',
    ],
    useAsButton: true,
    components: {

        // Main components
        preview: true,
        opacity: false,
        hue: true,

        // Input / output Options
        interaction: {
            hex: true,
            rgba: true,
            hsla: false,
            hsva: false,
            cmyk: false,
            input: true,
            clear: false,
            save: false
        }
    }
};
