FROM ubuntu:22.04

# 必要なパッケージのインストール
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# GBDKのダウンロードとインストール
WORKDIR /opt
RUN wget https://github.com/gbdk-2020/gbdk-2020/releases/download/4.1.1/gbdk-linux64.tar.gz \
    && tar -xzf gbdk-linux64.tar.gz \
    && rm gbdk-linux64.tar.gz

# 環境変数の設定
ENV GBDK_HOME=/opt/gbdk
ENV PATH="${GBDK_HOME}/bin:${PATH}"

# 作業ディレクトリ
WORKDIR /work

# ビルドコマンド
CMD ["make"]
