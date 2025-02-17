# Raytraced Cel-Look

このレポジトリーは東京工科大学の八王子キャンパスで作成した研究のソースコードになっています。
研究開始から今に至るまでのほとんどを公開しています。

## 目次

- [Raytraced Cel-Look](#raytraced-cel-look)
	- [目次](#目次)
	- [研究紹介](#研究紹介)
	- [機能と大まかなプラン](#機能と大まかなプラン)
	- [レポジトリー構造と説明](#レポジトリー構造と説明)
	- [ビルド](#ビルド)
		- [Windows](#windows)
		- [MacOS](#macoS)
		- [Linux](#linux)
	- [実行](#実行)
	- [参考文献](#参考文献)
	- [ライセンス](#ライセンス)

 ## 研究紹介

下記の文書が自分の研究の紹介になります　（まだどこでも投稿はしていないがアブストラクトみたいなもの捉えても構いません）

 > 連年レアルタイムレートレーシングを実装するゲーム作品が増え続ける中、 昨年で始めてレートレーシングでしか動かさないゲーム作品として「インディ・ジョーンズ/大いなる円環」が現れました。
 レートレーシングが今後も使われると言う傾向を見せると同時に、それが写実的な表現を求めている結果と言う傾向も見受けられます。
 しかし、作られているゲームが全て写実的な表現を求めている訳ではなく、PixarのRendermanやDreamworksのMoonRayで照明されるように、レートレーシング技術も色んな表現に使えることもできます。
 本研究では、セルルックを事例として使って、リアルタイム非写実的レートレーサーの実装とその利点と減点を定まるを目的としています。

難しく聞こえてくるかもしれませんが、正直に言うと、本当に軽い気持ちで「レートレーシングとセルルックを習うに興味があって、混ぜればどうなるんだろう」みたいな感覚で始めた研究です。
 
上にも書いている通り、非写実レートレーシングは新しい発送どころか、有名なアニメーションスタジオ（PixarのRenderManやDreamworksのMoonRayやAdobeのArnold）のレンダリングエンジンが既に出来ています。
唯一違いとすれば、本研究は日本のアニメのようなセルルックを目指していて（実装されているToonShadingできると思うんですけど、はっきりとしてはない）、リアルタイムを重視しています。

この研究は主にゲームやバーチャル配信かライブなどで使われる技術とし考えていますので、画質向上と新な表現の発見を目指しながら性能を念に置いています。

## 機能と大まかなプラン

レアルタイム性能を目指しているので、Vulkanを使ってのハイブリッドレートレーサーの実装を目的としています。
しかし、その前にこのレポジトリーは自分が技術やツールをよく理解できるようなサンドボックスでもあります。
なので、このレポジトリーでは本研究とは関係ないsceneや機能もあります。
下記が現段階で実装されているsceneの全てになります。

- ラスタライズされる三角形　(VulkanのグラフィクスAPIを馴染めるため)
- ラスタライズされるgltfモデル　(Zバッファ, インプットレイアウトなどモデルを表示するための必要な機能を馴染めるため)
- Peter Shirleyの[Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html)を基にCPUでのレートレーサーの作成 (レートレーシングの原則を理解するため)
- Peter Shirleyの[Raytracing In One Week-end](https://raytracing.github.io/books/RayTracingInOneWeekend.html)を基にGPUでのレートレーサーの作成 (「Vulkan Raytracing」を使用して)
- 基本的なセルルック（ToonShading）を実装する遅延レンダラー(Vulkanでの遅延レンダリングはどのようにするのかを理解するため)
- リアルタイム性能を考えてのハイブリッドレートレーサーの実装

恐らく最後の二つしか本当に研究に関係があります。

下記が実装されている機能になります。

- [x] ｇｌｆｗでのクロスプラットフォームウインドウ管理
- [x] 基本的なscene管理
- [x] 着脱と移動可能なUIとsceneごとの操作パラメター
- [x] まとめられたカメラ設定と3Dsceneでのカメラの自由操作

ラストライザーセルルックと研究の目標のレートレーサーセルルックを比べるためには、必要なのは：

- [ ] ちゃんとしたセルルックの開発　(多分大人気の[NiloToonURP](https://github.com/ColinLeung-NiloCat/UnityURPToonLitShaderExample)を例にします)
- [ ] ちゃんとしたハイブリッドレートレーサの開発
- [ ] 「セルルック」と言ものが何なのかを研究して、それをレートレーサーに組み込む
- [ ] 最後に比べて、利点と減点を述べる

みたいなプランになります。

 ## レポジトリー構造と説明

レポジトリーの構造は下記の通り：

- ルート
	- [deps](deps/) : この研究が依存している物tが全て入っているフォルダー
		- [bin](deps/bin/) : この研究が依存しているダイナミックライブラリー
		- [include](deps/include/) : この研究が依存しているインクルード
		- [libs](deps/libs/) : この研究が依存している静的ライブラリー
		- [src](deps/src/) : この研究が依存している、コンパリルが必要なソースコード
	- [include](include/) : 自分が研究で開発したインクルード
	- [media](media/) : この研究が使っているメディア　(モデルやテキスチャーなど)
	- [src](src/) : 自分が研究で開発したソースコード

この研究が依存しているサードパーティー技術や物は[deps](deps/)で、動くため必要なメディアは[media](media/)フォルダーに分けられていて、全てに違うライセンスがあります。（基本的にオープンソースで見つけた物なので、問題ないですけど、念のためご確認ください）。
自分が作成したファイルは全て[src](src/)と [include](include/)フォルダーにあります。これはこのレポジトリーのライセンスに属する物になります。

下記がこの研究が依存しているサードパーティーになります。
- deps
	- [GLFW](https://www.glfw.org/)　クロスプラットフォームウインドウ管理
	- [Dear ImGUI](https://github.com/ocornut/imgui)　簡単なUI
	- [Rapid Json](https://rapidjson.org/)　ｇｌｔｆローディングに必要なjsonパージング
	- [stb_image](https://github.com/nothings/stb/tree/master)　jpegローディング
	- [tiny_gltf](https://github.com/syoyo/tinygltf)　gltfローディング
	- [tiny_obj](https://github.com/tinyobjloader/tinyobjloader)　objローディング
- media
	- [the COLLADA Duck](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/Duck)　ソニーが[SCEA Shared Source License, Version 1.0](https://spdx.org/licenses/SCEA.html)のライセンスで配布しているアヒルのモデル
	- [the Utah Teapot](https://graphics.cs.utah.edu/teapot/)　(多分ライセンスなし)

リアルタイム性能を鑑みて、ハードウエアアクセラレーションを使用するレートレーシングが使える最新のグラフィクスAPIを使わざるを得ません。その中だと：
  - DirectX12(DirectX Raytracingを使って)
  - Vulkan(Vulkan Raytracingを使って)
  - Metal

クロスプラットフォームのためにVulkanを選びました。なので、VulkanSDKも使っています。

最後に、この研究で[GPSnoopyの「Raytracing in Vulkan」](https://github.com/GPSnoopy/RayTracingInVulkan)を凄く参考にしました、特にｇｌｓｌレートレースシェダーのシンタックス。
自分が参照にした物や記事は全て[references](README.md#reference-&-resources)で見学できますが、特に参考にしましたので先に述べていただきました。

## ビルド 
	
この研究がVulkanを使って開発されたため、ほとんどのOSでコンパイルして実行できると思いますが、全てのOSでテストを行った訳ではないのでご了承ください。
当たり前ながらも、コンパイルできるために各OSでの[Vulkan SDK](https://www.vulkan.org/tools#download-these-essential-development-tools)をインストールしなけてはなりません。

クロスプラットフォームコンパイルを可能にするべく、CMakeを使用しました。
でもCMakeだけで、どんな方法でもコンパイルできるとは限りませんので、そのためにクロスプラットフォーム性能が高いClangコンパイラーを使いましたので推奨します。（CMakeのC/CXXコンパイラー設定で付けるようを忘れずに）

### Windows

主にWindowsで開発したが、CMakeのツールを使った訳ではございません。Visual Studio ２０２２のCMakeツールを使って開発してました。（ルートディレクトリにあるCMakePresets.jsonはそのためです）。
なので、その方法を推奨します。

直接CMakeを使いたいのであれば、方法はいくつかありますが、Visual Studioをジェネレータでｍｓｖｃをコンパイラーを使いたいのであれば：

```bash
cmake.exe -G "[使いたいVisual Studioのバージョン]" -B out/ -S .
```

一回試して、出来たので多分今も問題ないと思います。

自分が使ったNinjaをジェネレータとclangをコンパイラーを使いたいとすると、改めてVisual Studio ２０２２のCMakeツールをインストールしてプロジェクトをルートディレクトリで開いてコンパイルすると言う方法を推奨します。

自分のツールでコンパイルしたいのであれば、試してもいいですけど、自分では出来なかったので、ちょっと試行錯誤が必要になるとおもいなす。
Visual StudioのツールがNinjaを使ってアウトプットディレクトリとCMakeのキャッシュを構築しています。
生成されるコマンドは下記に近いです：

```bash
cmake.exe -G "Ninja" -B out/ -S . -DCMAKE_C_COMPILER="cl.exe" -DCMAKE_CXX_COMPILER="cl.exe"
```

コマンドだけだと、Visual Studioが付ける環境が見えないので、それだけだと不足になります。
そして、Windowsの環境変数にVK_SDK_PATHをインストールした[Vulkan SDK](https://www.vulkan.org/tools#download-these-essential-development-tools)のディレクトリを付けるようを忘れずに。

プロジェクトが生成された後はコンパイルは下記のコマンドで：
```bash
cmake.exe --build out/ -DCMAKE_BUILD_TYPE="Debug"
```

```bash
cmake.exe --build out/ -DCMAKE_BUILD_TYPE="Release"
```

デバッグかリリースでコンパイルできます。
VisualStudio内だとコマンドは必要ありません。普通にVisualStudioでのビルド機能を使ってコンパイルしたら大丈夫です。

### MacOS

Macでのビルドは思ったより単純です。

勿論、CMakeをインストールしなけてはなりません。（自分の場合だとHomebrewでやりました）。
AppleはすでにAppleClangをデフォルトコンパイラーとして使っているのでclangをインストール必要がないはず。（なかったら、インストールがひつようです）。

MacOSでのデフォルトジェネレータのMakefileを使いたいなら：

```bash
cmake -B out/ -S .
```

で大丈夫です。
自分はXCodeを使う方が好きです。（XCodeがインストールされているの話しですけどね）。
それだと：

```bash
cmake -G="XCode" -B out/ -S .
```

XCodeはデバッグ機能が充実していて。ｇｄｂやＭｅｔａｌのＦｒａｍｅ Debuggerが便利ですけど、別に使わないのであれば、どっちでもいいと思います。

プロジェクトが生成される後は、Windowsと同じ: 

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Debug"
```

か

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Release"
```

デバッグかリリースでコンパイルできます。

XCodeだとXCode内でのビルドコマンドでできます。

### Linux

唯一試していないOSはLinuxとそのディストリビューションですが、Macとそんなに違いないと思います。

LinuxだとMakefileをジェネレータに使ってのプロジェクト生成がいいかと。
コマンドとしては、デフォルトの：

```bash
cmake -B out/ -S .
```

で足りるかどうかは不明ですけど、注意すべき点を述べると：
 - VulkanSDKをインストールすること
 - clangでコンパイルすること
 - 依存しているライブラリーを付けること
が必要になると思います。

ｇｃｃだとコンパイルできないと思うので他のＯＳだとclangでコンパイルできたので、直接clangでコンパイルするのが早いと思います。
そして、ライブラリーの件にしましては、[deps](deps/)のディレクトリでWindowsやMacOSの研究が依存しているサードパーティーのダイナミックライブラリーや静的ライブラリーを付けたのでっすけど、OSごとにフォーマットが異なるのでLinuxの方を探さないと行けないと思います。

いつも通り、プロジェクトが生成された後は、普通に：

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Debug"
```

か

```bash
cmake --build out/ -DCMAKE_BUILD_TYPE="Release"
```

デバッグかリリースでコンパイルできると思います。

## 実行

プラットフォーム問わず唯一考慮しないといけないのは充実ファイルとメディアディレクトリの居場所。実行のためのリーソースがはいってるので。（今のところWindowsの方の相対パスになってます）

ビルド方法によって実行ファイルの居場所が異なります。
上に載っているコマンドを使ったであればoutのディレクトリで産出されましたが、そのディレクトリのルートかどうかはジェネレータによって異なります。
CMakeLists.txtでダイナミックライブラリーは自動で実行ファイルの隣にコピーされるので実行自体は問題ないと思います。

ダブルクリックやコマンドなど、好きな方法で実行してください。

```bash
./[実行ファイルまでのパス]/RaytracedCel[.exe]
```

## 参考文献

申し訳ないですけど、自分が参考した記事や研究は英語が多いので[英語版の方のREADMEを参照してください](README.md#reference--resources)。

## ライセンス

上にも書いている通り、[src](src/)と[include](include/)に入っている全ては自分で作成しましたので、このレポジトリーのライセンスのMIT Licenseの対象となります。
詳しくは[LICENSE](LICENSE)をご確認ください。（えいごになります）

あとは自分が作成したものではないし各々のライセンスがあいますのでご確認いただければと思います。
[レポジトリー構造と説明](#レポジトリー構造と説明)で少しこの件に関してに触れていますのでそっちも宜しければご確認ください。