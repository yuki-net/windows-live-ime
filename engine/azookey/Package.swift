// swift-tools-version: 6.1

import PackageDescription

let package = Package(
    name: "WindowsLiveImeEngine",
    products: [
        .executable(name: "engine-host", targets: ["EngineHostCLI"]),
    ],
    dependencies: [
        .package(
            url: "https://github.com/azooKey/AzooKeyKanaKanjiConverter",
            revision: "80b8204f1cdfb364bb2ed355cf52c7ebb2519a0c"
        ),
    ],
    targets: [
        .target(
            name: "EngineCore",
            dependencies: [
                .product(
                    name: "KanaKanjiConverterModuleWithDefaultDictionary",
                    package: "AzooKeyKanaKanjiConverter"
                ),
            ]
        ),
        .target(
            name: "EngineHost",
            dependencies: ["EngineCore"]
        ),
        .executableTarget(
            name: "EngineHostCLI",
            dependencies: ["EngineCore", "EngineHost"]
        ),
        .testTarget(
            name: "EngineCoreTests",
            dependencies: ["EngineCore"]
        ),
    ],
    swiftLanguageModes: [.v6]
)
