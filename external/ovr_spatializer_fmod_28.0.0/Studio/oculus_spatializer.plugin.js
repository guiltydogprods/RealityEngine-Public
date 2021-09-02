studio.plugins.registerPluginDescription("Oculus Spatializer", {
    companyName: "Oculus",
    productName: "Oculus Spatializer",
    parameters: {
        "V. Radius": { displayName: "Radius" },
    },

    deckUi: {
        deckWidgetType: studio.ui.deckWidgetType.Layout,
        layout: studio.ui.layoutType.HBoxLayout,
        spacing: 8,
        items: [
			{
                deckWidgetType: studio.ui.deckWidgetType.Layout,
                layout: studio.ui.layoutType.VBoxLayout,
				contentsMargins: { left: 6, right: 6 },
				spacing: 2,
                items: [
					{ deckWidgetType: studio.ui.deckWidgetType.Pixmap, filePath: __dirname + "/oculus_vertical.png" },
					{
						deckWidgetType: studio.ui.deckWidgetType.Layout,
						layout: studio.ui.layoutType.HBoxLayout,
						spacing: 14,
						items: [
							{ deckWidgetType: studio.ui.deckWidgetType.Button, binding: "Reflections", },
							{ deckWidgetType: studio.ui.deckWidgetType.Button, binding: "Attenuation", },
                            { deckWidgetType: studio.ui.deckWidgetType.Dial, binding: "V. Radius", },
                            { deckWidgetType: studio.ui.deckWidgetType.Dial, binding: "Reverb Send" },
						],
					},
					{
						deckWidgetType: studio.ui.deckWidgetType.MinMaxFader,
						minimumBinding: "Range Min",
						maximumBinding: "Range Max",
						text: "Min & Max Distance",
						maximumHeight: 64,
					},
				],
			},
			{ deckWidgetType: studio.ui.deckWidgetType.OutputMeter, },
        ],
    },
});

studio.plugins.registerPluginDescription("Oculus Ambisonics", {
    companyName: "Oculus",
    productName: "Oculus Ambisonics",
    parameters: {},

    deckUi: {
        deckWidgetType: studio.ui.deckWidgetType.Layout,
        layout: studio.ui.layoutType.VBoxLayout,
        contentsMargins: { left: 6, right: 6, top: 6 },
        spacing: 40,
        items: [
            { deckWidgetType: studio.ui.deckWidgetType.Pixmap, filePath: __dirname + "/oculus.png", },
        ],
    }
});


studio.plugins.registerPluginDescription("Oculus Spatial Reverb", {
    companyName: "Oculus",
    productName: "Oculus Spatial Reverb",
    parameters: {
        "Refl. Engine": { displayName: "Reflections" },
        "Room Width":  { displayName: "Width" },
        "Room Height": { displayName: "Height" },
        "Room Length": { displayName: "Length" },
        "Refl. Left":  { displayName: "Left" },
        "Refl. Right": { displayName: "Right" },
        "Refl. Up":    { displayName: "Up" },
        "Refl. Down":  { displayName: "Down" },
        "Refl. Back":  { displayName: "Back" },
        "Refl. Front": { displayName: "Front" },
        "Wet Level":   { displayName: "Wet Level (dB)" },
    },

    deckUi: {
        deckWidgetType: studio.ui.deckWidgetType.Layout,
        layout: studio.ui.layoutType.HBoxLayout,
        contentsMargins: { left: 6, right: 6 },
        spacing: 12,
        items: [
        { deckWidgetType: studio.ui.deckWidgetType.Pixmap, filePath: __dirname + "/oculus_vertical.png" },
		{
			deckWidgetType: studio.ui.deckWidgetType.Layout,
			layout: studio.ui.layoutType.HBoxLayout,
			contentsMargins: { left: 0, right: 0 },
			spacing: 8,
            items: [
                {
                    deckWidgetType: studio.ui.deckWidgetType.Layout,
                    layout: studio.ui.layoutType.VBoxLayout,
                    contentsMargins: { left: 0, right: 0 },
                    spacing: 14,
                    items: [
                        { deckWidgetType: studio.ui.deckWidgetType.Dial, binding: "Wet Level", },
                        { deckWidgetType: studio.ui.deckWidgetType.NumberBox, binding: "Voice Limit", },
                    ]
                },
				{
					deckWidgetType: studio.ui.deckWidgetType.Layout,
					layout: studio.ui.layoutType.VBoxLayout,
					contentsMargins: { left: 0, right: 0 },
					spacing: 14,
					items: [
                        {
                            deckWidgetType: studio.ui.deckWidgetType.Layout,
                            layout: studio.ui.layoutType.HBoxLayout,
                            contentsMargins: { left: 0, right: 0 },
                            spacing: 12,
                            items: [
                                {
                                    deckWidgetType: studio.ui.deckWidgetType.Layout,
                                    layout: studio.ui.layoutType.VBoxLayout,
                                    contentsMargins: { left: 0, right: 0 },
                                    spacing: 4,
                                    items: [
                                        { deckWidgetType: studio.ui.deckWidgetType.Spacer, maximumHeight: 16 },
							            { deckWidgetType: studio.ui.deckWidgetType.Button, binding: "Refl. Engine", },
							            { deckWidgetType: studio.ui.deckWidgetType.Button, binding: "Reverb", },
                                    ]
                                },
                                {
                                    deckWidgetType: studio.ui.deckWidgetType.Layout,
                                    layout: studio.ui.layoutType.VBoxLayout,
                                    contentsMargins: { left: 0, right: 0 },
                                    spacing: 4,
                                    items: [
                                        { deckWidgetType: studio.ui.deckWidgetType.Label, text: "Room Dimensions", },
                                        {
                                            deckWidgetType: studio.ui.deckWidgetType.Layout,
                                            layout: studio.ui.layoutType.HBoxLayout,
                                            contentsMargins: { left: 0, right: 0 },
                                            spacing: 12,
                                            items: [
							                    { deckWidgetType: studio.ui.deckWidgetType.Dial, maximumHeight: 64, binding: "Room Width", },
							                    { deckWidgetType: studio.ui.deckWidgetType.Dial, maximumHeight: 64, binding: "Room Height", },
							                    { deckWidgetType: studio.ui.deckWidgetType.Dial, maximumHeight: 64, binding: "Room Length", },
                                            ]
                                        },
                                    ]
                                },
                            ]
                        },
			            {
			                deckWidgetType: studio.ui.deckWidgetType.MinMaxFader,
			                minimumBinding: "Range Min",
			                maximumBinding: "Range Max",
			                text: "Min & Max Distance",
			                maximumHeight: 64,
			            },
					]
				},
                {
                    deckWidgetType: studio.ui.deckWidgetType.Layout,
                    layout: studio.ui.layoutType.VBoxLayout,
                    contentsMargins: { left: 0, right: 0 },
                    spacing: 9,
                    items: [
                        { deckWidgetType: studio.ui.deckWidgetType.Label, text: "Wall Reflection Strength", },
                        {
                            deckWidgetType: studio.ui.deckWidgetType.Layout,
                            layout: studio.ui.layoutType.HBoxLayout,
                            spacing: 14,
                            items: [
							    { deckWidgetType: studio.ui.deckWidgetType.Fader, binding: "Refl. Left", },
							    { deckWidgetType: studio.ui.deckWidgetType.Fader, binding: "Refl. Right", },
							    { deckWidgetType: studio.ui.deckWidgetType.Fader, binding: "Refl. Up", },
							    { deckWidgetType: studio.ui.deckWidgetType.Fader, binding: "Refl. Down", },
							    { deckWidgetType: studio.ui.deckWidgetType.Fader, binding: "Refl. Back", },
							    { deckWidgetType: studio.ui.deckWidgetType.Fader, binding: "Refl. Front", },
                            ],
                        },
                    ],
                },
			],
		},
		{ deckWidgetType: studio.ui.deckWidgetType.OutputMeter, },
        ],
    }
});
