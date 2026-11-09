# Explorer Daily Groups

Windows 11 Explorerで、更新日時または作成日時を日別にグループ化するWindhawk Modの試作です。Mod IDは `explorer-daily-groups` です。Explorerでの日別表示・並べ替え・折りたたみは未検証です。

## ダウンロードと導入

1. [GitHub Actions](https://github.com/Sanowo0390/Explorer-Daily-Groups/actions/workflows/artifact.yml)で成功した実行を開き、Artifacts欄の **Explorer-Daily-Groups-experimental** をダウンロードします。
2. ZIPを展開し、`explorer-daily-groups.wh.cpp` をWindhawkの新規Mod編集画面に貼り付けてコンパイルします。
3. Explorerのフォルダーウィンドウを全て閉じ、Mod設定の **Enable daily grouping (experimental)** を有効にします。初期状態は無効です。
4. 検証用フォルダーを開き、「グループ化」から更新日時または作成日時を選びます。

設定変更や無効化の前にも、Explorerのフォルダーウィンドウを全て閉じてください。調査対象のDLL版と必要なシンボルはソースに記載しています。実機検証の項目は [テスト方針](TEST-PLAN.ja.md) を参照してください。

## アプリの「開く」「フォルダーの選択」ダイアログ

既定の対象は `explorer.exe` です。別プロセスのダイアログで「今日」「先月」などの見出しに戻る場合は、そのダイアログを表示する実行ファイルを個別に追加して試せます。

1. 対象アプリの実行ファイル名を確認し、選択ダイアログを閉じます。
2. Windhawkで **Explorer Daily Groups → 高度 → カスタムプロセス対象リスト** を開き、対象の `.exe` 名またはパスを1行追加して保存します。
3. **Mod の対象リスト、除外リストを無視する** はオフにして、Explorerにも適用します。
4. 対象アプリを再起動し、ダイアログで「グループで表示 → 変更日」を選びます。

ダイアログでの動作はアプリごとに確認が必要です。表示が変わらない場合は、Modのログを有効にし、対象の実行ファイル名と `Init failed:` または `First hit:` の行を確認してください。元に戻すには、追加した1行を削除して対象アプリを再起動します。

Actionsはソース、資料、テスト、チェックサムを配布します。Windhawkでのコンパイルと実機テストは別途行います。
