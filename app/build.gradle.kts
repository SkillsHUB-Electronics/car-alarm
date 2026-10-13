plugins {
    id("com.android.application") version "8.5.2" apply false
    id("org.jetbrains.kotlin.android") version "1.9.24" apply false
}

// OneDrive locks files inside a synced project folder, which makes Gradle fail while cleaning its build
// directory. Set CARALARM_BUILD_DIR to a folder outside OneDrive to build there instead (see BUILD.md).
System.getenv("CARALARM_BUILD_DIR")?.let { base ->
    allprojects {
        layout.buildDirectory.set(file("$base/${project.name}"))
    }
}
