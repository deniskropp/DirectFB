wget https://nodeload.github.com/cdave1/freetype2-android/tarball/master -O freetype2-android.tar.gz
mkdir -p freetype2-android
tar xvzf freetype2-android.tar.gz -C freetype2-android --strip-components=1
android update project --path . --target android-10
